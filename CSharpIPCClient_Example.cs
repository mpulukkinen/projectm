/**
 * C# IPC CLIENT EXAMPLE
 *
 * This is a complete example of how to communicate with the C++ projectM app
 * using the IPC protocol over stdin/stdout.
 *
 * Usage:
 * 1. Start the C++ app as a child process with stdin/stdout redirected
 * 2. Create an instance of this class
 * 3. Send/receive messages as needed
 */

using System;
using System.Diagnostics;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using System.Collections.Generic;
using Newtonsoft.Json.Linq;
using Newtonsoft.Json;

namespace ProjectMIPC
{
    /// <summary>
    /// Message types for IPC communication
    /// </summary>
    public enum MessageType
    {
        // C# -> C++
        TIMESTAMP = 0,
        LOAD_PRESET = 1,
        DELETE_PRESET = 2,
        START_PREVIEW = 3,
        STOP_PREVIEW = 4,

        // C++ -> C#
        PRESET_LOADED = 5,
        CURRENT_STATE = 6,
        PREVIEW_STATUS = 7,
        ERROR_RESPONSE = 8
    }

    /// <summary>
    /// Represents a preset in the queue
    /// </summary>
    public class PresetQueueEntry
    {
        public string PresetName { get; set; }
        public ulong TimestampMs { get; set; }

        public PresetQueueEntry(string name, ulong timestamp)
        {
            PresetName = name;
            TimestampMs = timestamp;
        }
    }

    /// <summary>
    /// Main IPC client class
    /// </summary>
    public class ProjectMIPCClient : IDisposable
    {
        private Process cppProcess;
        private StreamWriter processInput;
        private StreamReader processOutput;
        private Task listenerTask;
        private CancellationTokenSource cancellationSource;

        // Event for receiving messages
        public event EventHandler<IPCMessageEventArgs> MessageReceived;

        // Current state
        public List<PresetQueueEntry> PresetQueue { get; private set; }
        public ulong LastTimestampMs { get; private set; }
        public bool IsPreviewPlaying { get; private set; }

        /// <summary>
        /// Initialize the IPC client by starting the C++ process
        /// </summary>
        /// <param name="exePath">Path to the C++ executable</param>
        /// <param name="args">Command line arguments</param>
        public ProjectMIPCClient(string exePath, string args = "")
        {
            PresetQueue = new List<PresetQueueEntry>();
            cancellationSource = new CancellationTokenSource();

            try
            {
                // Start the C++ process
                ProcessStartInfo psi = new ProcessStartInfo
                {
                    FileName = exePath,
                    Arguments = args,
                    RedirectStandardInput = true,
                    RedirectStandardOutput = true,
                    RedirectStandardError = false,
                    UseShellExecute = false,
                    CreateNoWindow = false
                };

                cppProcess = Process.Start(psi);
                processInput = cppProcess.StandardInput;
                processOutput = cppProcess.StandardOutput;

                // CRITICAL: Set stream buffering to line-buffered (unbuffered text)
                processInput.AutoFlush = true;  // Auto-flush after each write
                processOutput.BaseStream.ReadTimeout = 5000;  // 5 second timeout

                // Start listening for messages
                listenerTask = Task.Run(() => ListenForMessages(cancellationSource.Token));

                // Give the C++ app a moment to initialize IPC
                Task.Delay(500).Wait();

                Console.WriteLine("IPC Client initialized successfully");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Failed to initialize IPC client: {ex.Message}");
                throw;
            }
        }

        /// <summary>
        /// Send current timestamp to C++ app
        /// </summary>
        public void SendTimestamp(ulong timestampMs)
        {
            try
            {
                JObject msg = new JObject();
                msg["type"] = (int)MessageType.TIMESTAMP;
                msg["data"] = new JObject
                {
                    { "timestampMs", timestampMs }
                };

                SendMessage(msg.ToString(Formatting.None));
                LastTimestampMs = timestampMs;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error sending timestamp: {ex.Message}");
            }
        }

        /// <summary>
        /// Load a preset at specific timestamp
        /// </summary>
        public void LoadPreset(string presetName, ulong startTimestampMs)
        {
            try
            {
                JObject msg = new JObject();
                msg["type"] = (int)MessageType.LOAD_PRESET;
                msg["data"] = new JObject
                {
                    { "presetName", presetName },
                    { "startTimestampMs", startTimestampMs }
                };

                SendMessage(msg.ToString(Formatting.None));
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error loading preset: {ex.Message}");
            }
        }

        /// <summary>
        /// Delete a preset from the queue
        /// </summary>
        public void DeletePreset(string presetName, ulong timestampMs)
        {
            try
            {
                JObject msg = new JObject();
                msg["type"] = (int)MessageType.DELETE_PRESET;
                msg["data"] = new JObject
                {
                    { "presetName", presetName },
                    { "timestampMs", timestampMs }
                };

                SendMessage(msg.ToString(Formatting.None));
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error deleting preset: {ex.Message}");
            }
        }

        /// <summary>
        /// Start audio preview from given timestamp
        /// </summary>
        public void StartPreview(ulong fromTimestampMs)
        {
            try
            {
                JObject msg = new JObject();
                msg["type"] = (int)MessageType.START_PREVIEW;
                msg["data"] = new JObject
                {
                    { "fromTimestampMs", fromTimestampMs }
                };

                SendMessage(msg.ToString(Formatting.None));
                IsPreviewPlaying = true;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error starting preview: {ex.Message}");
            }
        }

        /// <summary>
        /// Stop audio preview
        /// </summary>
        public void StopPreview()
        {
            try
            {
                JObject msg = new JObject();
                msg["type"] = (int)MessageType.STOP_PREVIEW;
                msg["data"] = new JObject();

                SendMessage(msg.ToString(Formatting.None));
                IsPreviewPlaying = false;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error stopping preview: {ex.Message}");
            }
        }

        /// <summary>
        /// Internal: Send raw message
        /// </summary>
        private void SendMessage(string jsonMessage)
        {
            try
            {
                if (processInput == null)
                {
                    throw new InvalidOperationException("Process input stream is null");
                }

                if (processInput.BaseStream == null || !processInput.BaseStream.CanWrite)
                {
                    throw new InvalidOperationException("Cannot write to process - stream is closed");
                }

                Console.WriteLine($"[C# -> C++] Sending: {jsonMessage}");
                processInput.WriteLine(jsonMessage);
                processInput.Flush();  // Ensure message is sent immediately
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error sending message: {ex.Message}");
                throw;
            }
        }

        /// <summary>
        /// Listen for messages from C++ process
        /// </summary>
        private void ListenForMessages(CancellationToken cancellationToken)
        {
            try
            {
                while (!cancellationToken.IsCancellationRequested)
                {
                    string line = processOutput?.ReadLine();
                    if (string.IsNullOrEmpty(line))
                    {
                        break;  // Process ended or no more input
                    }

                    try
                    {
                        JObject msg = JObject.Parse(line);
                        MessageType type = (MessageType)msg["type"].Value<int>();
                        JObject data = msg["data"] as JObject;

                        HandleMessage(type, data);
                    }
                    catch (Exception ex)
                    {
                        Console.WriteLine($"Error parsing message: {ex.Message}");
                    }
                }
            }
            catch (Exception ex)
            {
                if (!cancellationToken.IsCancellationRequested)
                {
                    Console.WriteLine($"Listener thread error: {ex.Message}");
                }
            }
        }

        /// <summary>
        /// Handle incoming messages from C++
        /// </summary>
        private void HandleMessage(MessageType type, JObject data)
        {
            switch (type)
            {
                case MessageType.PRESET_LOADED:
                    HandlePresetLoaded(data);
                    break;

                case MessageType.CURRENT_STATE:
                    HandleCurrentState(data);
                    break;

                case MessageType.PREVIEW_STATUS:
                    HandlePreviewStatus(data);
                    break;

                case MessageType.ERROR_RESPONSE:
                    HandleError(data);
                    break;

                default:
                    Console.WriteLine($"Unknown message type: {type}");
                    break;
            }

            // Raise event
            MessageReceived?.Invoke(this, new IPCMessageEventArgs
            {
                MessageType = type,
                Data = data
            });
        }

        private void HandlePresetLoaded(JObject data)
        {
            string presetName = data["presetName"]?.Value<string>();
            ulong timestamp = data["startTimestampMs"]?.Value<ulong>() ?? 0;
            Console.WriteLine($"[C++] Preset loaded: {presetName} at {timestamp}ms");
        }

        private void HandleCurrentState(JObject data)
        {
            ulong lastTimestamp = data["lastReceivedTimestampMs"]?.Value<ulong>() ?? 0;
            JArray presetsArray = data["presets"] as JArray;

            PresetQueue.Clear();
            if (presetsArray != null)
            {
                foreach (var preset in presetsArray)
                {
                    string name = preset["presetName"]?.Value<string>();
                    ulong ts = preset["timestampMs"]?.Value<ulong>() ?? 0;
                    PresetQueue.Add(new PresetQueueEntry(name, ts));
                }
            }

            Console.WriteLine($"[C++] Current state: {PresetQueue.Count} presets");
        }

        private void HandlePreviewStatus(JObject data)
        {
            bool isPlaying = data["isPlaying"]?.Value<bool>() ?? false;
            ulong currentTimestamp = data["currentTimestampMs"]?.Value<ulong>() ?? 0;
            IsPreviewPlaying = isPlaying;
            Console.WriteLine($"[C++] Preview status: Playing={isPlaying}, Timestamp={currentTimestamp}ms");
        }

        private void HandleError(JObject data)
        {
            string error = data["error"]?.Value<string>();
            Console.WriteLine($"[C++ ERROR] {error}");
        }

        /// <summary>
        /// Cleanup resources
        /// </summary>
        public void Dispose()
        {
            try
            {
                cancellationSource?.Cancel();
                listenerTask?.Wait(5000);  // Wait max 5 seconds

                processInput?.Dispose();
                processOutput?.Dispose();

                if (cppProcess != null && !cppProcess.HasExited)
                {
                    cppProcess.Kill();
                    cppProcess.WaitForExit(5000);
                }
                cppProcess?.Dispose();
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error during cleanup: {ex.Message}");
            }
        }
    }

    /// <summary>
    /// Event arguments for received messages
    /// </summary>
    public class IPCMessageEventArgs : EventArgs
    {
        public MessageType MessageType { get; set; }
        public JObject Data { get; set; }
    }
}
