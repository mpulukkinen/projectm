# C# Implementation Patterns & Best Practices

## Pattern 1: Simple Console Application

```csharp
using System;
using ProjectMIPC;

class Program
{
    static void Main()
    {
        // Start C++ process
        var client = new ProjectMIPCClient(
            "LvsAudioReactiveVisualizer.exe",
            "--preset-dir C:\\presets"
        );

        // Handle messages
        client.MessageReceived += OnMessageReceived;

        // Load some presets
        client.LoadPreset("intro.milk", 0);
        client.LoadPreset("verse.milk", 5000);
        client.LoadPreset("chorus.milk", 15000);

        // Start preview
        client.StartPreview(0);

        // Simulate audio playback
        for (ulong ms = 0; ms < 30000; ms += 50)
        {
            client.SendTimestamp(ms);
            System.Threading.Thread.Sleep(50);
        }

        // Cleanup
        client.Dispose();
    }

    static void OnMessageReceived(object sender,
        ProjectMIPC.IPCMessageEventArgs args)
    {
        Console.WriteLine($"[{args.MessageType}] {args.Data}");
    }
}
```

## Pattern 2: MVVM UI with WPF

```csharp
using System;
using System.Collections.ObjectModel;
using System.Windows.Input;
using ProjectMIPC;

public class PresetViewModel
{
    private ProjectMIPCClient ipcClient;

    public ObservableCollection<PresetQueueEntry> Presets { get; }

    public ICommand AddPresetCommand { get; }
    public ICommand DeletePresetCommand { get; }
    public ICommand StartPreviewCommand { get; }
    public ICommand StopPreviewCommand { get; }

    public PresetViewModel()
    {
        Presets = new ObservableCollection<PresetQueueEntry>();

        AddPresetCommand = new RelayCommand(AddPreset);
        DeletePresetCommand = new RelayCommand<PresetQueueEntry>(DeletePreset);
        StartPreviewCommand = new RelayCommand(StartPreview);
        StopPreviewCommand = new RelayCommand(StopPreview);

        InitializeIPC();
    }

    private void InitializeIPC()
    {
        ipcClient = new ProjectMIPCClient(
            "LvsAudioReactiveVisualizer.exe",
            "--preset-dir C:\\presets"
        );

        ipcClient.MessageReceived += (s, e) =>
        {
            if (e.MessageType == MessageType.CURRENT_STATE)
            {
                UpdatePresetList();
            }
        };
    }

    private void AddPreset(object param)
    {
        // param = (presetName, timestampSeconds)
        var (name, time) = ((string, double))param;
        ipcClient.LoadPreset(name, (ulong)(time * 1000));
    }

    private void DeletePreset(PresetQueueEntry preset)
    {
        if (preset != null)
        {
            ipcClient.DeletePreset(preset.PresetName, preset.TimestampMs);
        }
    }

    private void StartPreview(object param)
    {
        ipcClient.StartPreview(0);
    }

    private void StopPreview(object param)
    {
        ipcClient.StopPreview();
    }

    private void UpdatePresetList()
    {
        Presets.Clear();
        foreach (var preset in ipcClient.PresetQueue)
        {
            Presets.Add(preset);
        }
    }
}
```

## Pattern 3: Audio Player Integration

```csharp
using NAudio.Wave;
using ProjectMIPC;

public class AudioPlayerWithPresets : IDisposable
{
    private IWavePlayer wavePlayer;
    private AudioFileReader audioReader;
    private ProjectMIPCClient ipcClient;
    private System.Diagnostics.Stopwatch playbackTimer;

    public void Initialize(string audioFilePath, string presetDir)
    {
        // Setup audio
        wavePlayer = new WaveOutEvent();
        audioReader = new AudioFileReader(audioFilePath);
        wavePlayer.Init(audioReader);

        // Setup IPC
        ipcClient = new ProjectMIPCClient(
            "LvsAudioReactiveVisualizer.exe",
            $"--preset-dir {presetDir}"
        );

        ipcClient.MessageReceived += OnIPCMessage;
        playbackTimer = new System.Diagnostics.Stopwatch();
    }

    public void Play()
    {
        wavePlayer.Play();
        playbackTimer.Start();
        ipcClient.StartPreview(0);

        // Start update thread
        var updateThread = new System.Threading.Thread(UpdatePlaybackPosition)
        {
            IsBackground = true
        };
        updateThread.Start();
    }

    private void UpdatePlaybackPosition()
    {
        while (wavePlayer.PlaybackState == PlaybackState.Playing)
        {
            ulong currentMs = (ulong)audioReader.CurrentTime.TotalMilliseconds;
            ipcClient.SendTimestamp(currentMs);
            System.Threading.Thread.Sleep(50);  // Update every 50ms
        }
    }

    public void AddPreset(string presetName, double startTimeSeconds)
    {
        ipcClient.LoadPreset(presetName, (ulong)(startTimeSeconds * 1000));
    }

    public void Stop()
    {
        wavePlayer.Stop();
        playbackTimer.Stop();
        ipcClient.StopPreview();
    }

    private void OnIPCMessage(object sender,
        ProjectMIPC.IPCMessageEventArgs args)
    {
        // Handle messages
    }

    public void Dispose()
    {
        wavePlayer?.Dispose();
        audioReader?.Dispose();
        ipcClient?.Dispose();
    }
}

// Usage:
var player = new AudioPlayerWithPresets();
player.Initialize("music.mp3", "C:\\presets");
player.AddPreset("intro.milk", 0);
player.AddPreset("verse.milk", 5);
player.Play();
```

## Pattern 4: Preset Queue Editor

```csharp
using System;
using System.Collections.Generic;
using System.Linq;
using ProjectMIPC;

public class PresetQueueEditor
{
    private ProjectMIPCClient ipcClient;
    private List<PresetQueueEntry> workingQueue;

    public PresetQueueEditor(ProjectMIPCClient client)
    {
        ipcClient = client;
        workingQueue = new List<PresetQueueEntry>();
        SyncFromServer();
    }

    public void SyncFromServer()
    {
        workingQueue = new List<PresetQueueEntry>(ipcClient.PresetQueue);
    }

    public void AddPreset(string name, ulong timestampMs)
    {
        if (!workingQueue.Any(p => p.PresetName == name &&
                                     p.TimestampMs == timestampMs))
        {
            workingQueue.Add(new PresetQueueEntry(name, timestampMs));
            workingQueue = workingQueue.OrderBy(p => p.TimestampMs).ToList();
            ipcClient.LoadPreset(name, timestampMs);
        }
    }

    public void RemovePreset(int index)
    {
        if (index >= 0 && index < workingQueue.Count)
        {
            var preset = workingQueue[index];
            workingQueue.RemoveAt(index);
            ipcClient.DeletePreset(preset.PresetName, preset.TimestampMs);
        }
    }

    public void MovePreset(int fromIndex, int toIndex)
    {
        if (fromIndex >= 0 && fromIndex < workingQueue.Count &&
            toIndex >= 0 && toIndex < workingQueue.Count)
        {
            var preset = workingQueue[fromIndex];
            workingQueue.RemoveAt(fromIndex);
            workingQueue.Insert(toIndex, preset);
        }
    }

    public void ShiftTimestamp(int index, long deltaMs)
    {
        if (index >= 0 && index < workingQueue.Count)
        {
            var preset = workingQueue[index];
            var newTimestamp = (long)preset.TimestampMs + deltaMs;

            if (newTimestamp >= 0)
            {
                ipcClient.DeletePreset(preset.PresetName, preset.TimestampMs);
                ipcClient.LoadPreset(preset.PresetName, (ulong)newTimestamp);

                preset.TimestampMs = (ulong)newTimestamp;
                workingQueue = workingQueue.OrderBy(p => p.TimestampMs).ToList();
            }
        }
    }

    public List<PresetQueueEntry> GetQueue()
    {
        return new List<PresetQueueEntry>(workingQueue);
    }

    public TimeSpan GetTotalDuration()
    {
        if (workingQueue.Count == 0) return TimeSpan.Zero;
        return TimeSpan.FromMilliseconds(workingQueue.Last().TimestampMs);
    }
}

// Usage:
var editor = new PresetQueueEditor(client);
editor.AddPreset("intro.milk", 0);
editor.AddPreset("verse.milk", 5000);
editor.ShiftTimestamp(0, 1000);  // Shift first preset 1 second later
Console.WriteLine($"Total duration: {editor.GetTotalDuration()}");
```

## Pattern 5: Error Handling & Retry Logic

```csharp
using System;
using System.Threading.Tasks;
using ProjectMIPC;

public class ResilientIPCClient
{
    private ProjectMIPCClient ipcClient;
    private int reconnectAttempts = 0;
    private const int MAX_RECONNECT_ATTEMPTS = 3;
    private System.Diagnostics.Stopwatch connectionTimer;

    public async Task<bool> TryConnectAsync(string exePath, string args)
    {
        for (int attempt = 0; attempt < MAX_RECONNECT_ATTEMPTS; attempt++)
        {
            try
            {
                ipcClient = new ProjectMIPCClient(exePath, args);
                ipcClient.MessageReceived += OnMessageReceived;
                connectionTimer = new System.Diagnostics.Stopwatch();
                connectionTimer.Start();
                return true;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Connection attempt {attempt + 1} failed: {ex.Message}");
                await Task.Delay(1000 * (attempt + 1));  // Exponential backoff
            }
        }
        return false;
    }

    public void SendTimestampSafe(ulong timestampMs)
    {
        try
        {
            if (ipcClient != null && connectionTimer.ElapsedMilliseconds < 30000)
            {
                ipcClient.SendTimestamp(timestampMs);
            }
            else
            {
                Console.WriteLine("Connection lost or timeout");
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Error sending timestamp: {ex.Message}");
        }
    }

    public void LoadPresetSafe(string name, ulong timestampMs)
    {
        try
        {
            ipcClient?.LoadPreset(name, timestampMs);
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Error loading preset: {ex.Message}");
        }
    }

    private void OnMessageReceived(object sender,
        ProjectMIPC.IPCMessageEventArgs args)
    {
        connectionTimer.Restart();  // Reset timeout on activity

        if (args.MessageType == MessageType.ERROR_RESPONSE)
        {
            Console.WriteLine($"Remote error: {args.Data["error"]}");
        }
    }

    public void Dispose()
    {
        ipcClient?.Dispose();
    }
}

// Usage:
var resilientClient = new ResilientIPCClient();
if (await resilientClient.TryConnectAsync("LvsAudioReactiveVisualizer.exe", ""))
{
    resilientClient.LoadPresetSafe("test.milk", 5000);
}
```

## Pattern 6: Preset Sequencing

```csharp
using System;
using System.Collections.Generic;
using System.Linq;
using ProjectMIPC;

public class PresetSequencer
{
    private List<(string presetName, double durationSeconds)> sequence;
    private ProjectMIPCClient ipcClient;

    public PresetSequencer(ProjectMIPCClient client)
    {
        ipcClient = client;
        sequence = new List<(string, double)>();
    }

    public void AddSequence(string presetName, double durationSeconds)
    {
        sequence.Add((presetName, durationSeconds));
    }

    public void GenerateQueue()
    {
        double currentTimeSeconds = 0;

        foreach (var (presetName, durationSeconds) in sequence)
        {
            ipcClient.LoadPreset(presetName, (ulong)(currentTimeSeconds * 1000));
            currentTimeSeconds += durationSeconds;
        }
    }

    public double GetTotalDuration()
    {
        return sequence.Sum(x => x.durationSeconds);
    }
}

// Usage:
var sequencer = new PresetSequencer(client);
sequencer.AddSequence("intro.milk", 5);
sequencer.AddSequence("verse.milk", 10);
sequencer.AddSequence("chorus.milk", 15);
sequencer.AddSequence("outro.milk", 5);
sequencer.GenerateQueue();
Console.WriteLine($"Total duration: {sequencer.GetTotalDuration()} seconds");
```

## Pattern 7: Real-time Monitoring

```csharp
using System;
using System.Threading;
using ProjectMIPC;

public class PresetMonitor
{
    private ProjectMIPCClient ipcClient;
    private PresetQueueEntry currentPreset;

    public event EventHandler<PresetChangedEventArgs> PresetChanged;

    public PresetMonitor(ProjectMIPCClient client)
    {
        ipcClient = client;
        ipcClient.MessageReceived += OnMessage;
    }

    private void OnMessage(object sender,
        ProjectMIPC.IPCMessageEventArgs args)
    {
        if (args.MessageType == MessageType.CURRENT_STATE)
        {
            var presets = ipcClient.PresetQueue;
            if (presets.Count > 0)
            {
                var newPreset = presets.FirstOrDefault();
                if (currentPreset?.PresetName != newPreset?.PresetName)
                {
                    var oldPreset = currentPreset;
                    currentPreset = newPreset;

                    PresetChanged?.Invoke(this, new PresetChangedEventArgs
                    {
                        OldPreset = oldPreset,
                        NewPreset = newPreset,
                        ChangedAt = DateTime.Now
                    });
                }
            }
        }
    }
}

public class PresetChangedEventArgs : EventArgs
{
    public PresetQueueEntry OldPreset { get; set; }
    public PresetQueueEntry NewPreset { get; set; }
    public DateTime ChangedAt { get; set; }
}

// Usage:
var monitor = new PresetMonitor(client);
monitor.PresetChanged += (s, e) =>
{
    Console.WriteLine($"Preset changed from {e.OldPreset?.PresetName} " +
                      $"to {e.NewPreset?.PresetName}");
};
```

These patterns provide comprehensive examples for different use cases and architectures.
