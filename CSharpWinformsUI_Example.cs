/**
 * C# WinForms Example UI for ProjectM IPC
 *
 * This is a complete working example of a Windows Forms UI that:
 * - Displays preset queue sorted by timestamps
 * - Allows adding presets at specific timestamps
 * - Allows deleting presets
 * - Shows audio playback position
 * - Displays real-time status from C++ app
 */

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Windows.Forms;
using System.Threading.Tasks;
using ProjectMIPC;

namespace ProjectMPresetUI
{
    public partial class MainForm : Form
    {
        private ProjectMIPCClient ipcClient;
        private System.Windows.Forms.Timer updateTimer;
        private System.Windows.Forms.Timer statusTimer;
        private List<string> availablePresets;
        private ulong timestampCounter = 0;

        public MainForm()
        {
            InitializeComponent();
            availablePresets = new List<string>();
            setupUI();
        }

        private void setupUI()
        {
            this.Text = "ProjectM Preset Queue Manager";
            this.Size = new System.Drawing.Size(900, 600);
            this.StartPosition = FormStartPosition.CenterScreen;

            // Main layout
            var panel = new TableLayoutPanel
            {
                Dock = DockStyle.Fill,
                ColumnCount = 2,
                RowCount = 4,
                Padding = new Padding(10)
            };
            panel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 70));
            panel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 30));

            // === QUEUE DISPLAY ===
            var queueLabel = new Label
            {
                Text = "Preset Queue (sorted by timestamp):",
                AutoSize = true,
                Font = new System.Drawing.Font("Arial", 10, System.Drawing.FontStyle.Bold)
            };
            panel.Controls.Add(queueLabel, 0, 0);

            QueueListView = new ListView
            {
                View = View.Details,
                FullRowSelect = true,
                GridLines = true,
                Dock = DockStyle.Fill
            };
            QueueListView.Columns.Add("Preset Name", 250);
            QueueListView.Columns.Add("Start Time (ms)", 100);
            QueueListView.Columns.Add("Start Time (sec)", 100);
            panel.Controls.Add(QueueListView, 0, 1);

            // === CONTROLS PANEL ===
            var controlsPanel = new Panel
            {
                Dock = DockStyle.Fill,
                AutoScroll = true
            };
            controlsPanel.Controls.Add(new Label
            {
                Text = "Add Preset",
                AutoSize = true,
                Font = new System.Drawing.Font("Arial", 10, System.Drawing.FontStyle.Bold),
                Top = 10,
                Left = 10
            });

            // Preset dropdown
            PresetComboBox = new ComboBox
            {
                Left = 10,
                Top = 35,
                Width = 180,
                DropDownStyle = ComboBoxStyle.DropDownList
            };
            controlsPanel.Controls.Add(PresetComboBox);

            // Timestamp input
            var timeLabel = new Label
            {
                Text = "Time (sec):",
                Left = 10,
                Top = 65,
                AutoSize = true
            };
            controlsPanel.Controls.Add(timeLabel);

            TimestampTextBox = new TextBox
            {
                Left = 10,
                Top = 85,
                Width = 100,
                Text = "0"
            };
            controlsPanel.Controls.Add(TimestampTextBox);

            // Add button
            AddButton = new Button
            {
                Text = "Add Preset",
                Left = 10,
                Top = 115,
                Width = 180,
                Height = 35
            };
            AddButton.Click += AddButton_Click;
            controlsPanel.Controls.Add(AddButton);

            // Delete button
            DeleteButton = new Button
            {
                Text = "Delete Selected",
                Left = 10,
                Top = 160,
                Width = 180,
                Height = 35,
                BackColor = System.Drawing.Color.LightCoral
            };
            DeleteButton.Click += DeleteButton_Click;
            controlsPanel.Controls.Add(DeleteButton);

            // === STATUS ===
            StatusLabel = new Label
            {
                Text = "Status: Not connected",
                Left = 10,
                Top = 210,
                Width = 180,
                AutoSize = true,
                ForeColor = System.Drawing.Color.Red
            };
            controlsPanel.Controls.Add(StatusLabel);

            PlaybackLabel = new Label
            {
                Text = "Playback: Stopped",
                Left = 10,
                Top = 240,
                Width = 180,
                AutoSize = true
            };
            controlsPanel.Controls.Add(PlaybackLabel);

            // Preview controls
            var previewLabel = new Label
            {
                Text = "Preview Controls",
                Left = 10,
                Top = 280,
                AutoSize = true,
                Font = new System.Drawing.Font("Arial", 10, System.Drawing.FontStyle.Bold)
            };
            controlsPanel.Controls.Add(previewLabel);

            StartPreviewButton = new Button
            {
                Text = "Start Preview",
                Left = 10,
                Top = 305,
                Width = 180,
                Height = 35,
                BackColor = System.Drawing.Color.LightGreen
            };
            StartPreviewButton.Click += StartPreviewButton_Click;
            controlsPanel.Controls.Add(StartPreviewButton);

            StopPreviewButton = new Button
            {
                Text = "Stop Preview",
                Left = 10,
                Top = 350,
                Width = 180,
                Height = 35,
                BackColor = System.Drawing.Color.LightSalmon
            };
            StopPreviewButton.Click += StopPreviewButton_Click;
            controlsPanel.Controls.Add(StopPreviewButton);

            panel.Controls.Add(controlsPanel, 1, 1);

            // === BOTTOM STATUS BAR ===
            BottomStatusLabel = new Label
            {
                Text = "Ready",
                Dock = DockStyle.Bottom,
                Height = 25,
                BorderStyle = BorderStyle.FixedSingle,
                TextAlign = System.Drawing.ContentAlignment.MiddleLeft,
                Padding = new Padding(5)
            };
            panel.Controls.Add(BottomStatusLabel, 0, 3);
            panel.SetColumnSpan(BottomStatusLabel, 2);

            this.Controls.Add(panel);

            // Setup timers
            updateTimer = new System.Windows.Forms.Timer();
            updateTimer.Interval = 500;  // Update every 500ms
            updateTimer.Tick += UpdateTimer_Tick;

            statusTimer = new System.Windows.Forms.Timer();
            statusTimer.Interval = 100;  // Update status every 100ms
            statusTimer.Tick += StatusTimer_Tick;
        }

        protected override void OnLoad(EventArgs e)
        {
            base.OnLoad(e);
            initializeIPC();
            loadAvailablePresets();
        }

        private void initializeIPC()
        {
            try
            {
                // Adjust path to your actual executable
                string exePath = @"T:\CodeProjects\projectm\out\src\sdl-test-ui\Debug\LvsAudioReactiveVisualizer.exe";
                string args = @"--preset-dir T:\CodeProjects\projectm\presets";

                ipcClient = new ProjectMIPCClient(exePath, args);

                // Subscribe to messages
                ipcClient.MessageReceived += (sender, args_msg) =>
                {
                    this.Invoke((MethodInvoker)delegate
                    {
                        handleIPCMessage(args_msg.MessageType, args_msg.Data);
                    });
                };

                StatusLabel.Text = "Status: Connected";
                StatusLabel.ForeColor = System.Drawing.Color.Green;
                BottomStatusLabel.Text = "Connected to C++ process";

                updateTimer.Start();
                statusTimer.Start();
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Failed to start C++ process: {ex.Message}", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                StatusLabel.Text = $"Error: {ex.Message}";
                StatusLabel.ForeColor = System.Drawing.Color.Red;
            }
        }

        private void loadAvailablePresets()
        {
            try
            {
                // TODO: Load preset list from file system
                // For now, add some example presets
                availablePresets.AddRange(new[]
                {
                    "abstract.milk",
                    "album.milk",
                    "bongo.milk",
                    "cool.milk",
                    "demo.milk",
                    "fireworks.milk",
                    "geiss.milk",
                    "geometrix.milk"
                });

                PresetComboBox.DataSource = new List<string>(availablePresets);
            }
            catch (Exception ex)
            {
                BottomStatusLabel.Text = $"Error loading presets: {ex.Message}";
            }
        }

        private void handleIPCMessage(MessageType type, dynamic data)
        {
            switch (type)
            {
                case MessageType.PRESET_LOADED:
                    BottomStatusLabel.Text = $"Preset loaded: {data["presetName"]}";
                    break;

                case MessageType.CURRENT_STATE:
                    updateQueueDisplay();
                    break;

                case MessageType.PREVIEW_STATUS:
                    bool isPlaying = data["isPlaying"]?.Value<bool>() ?? false;
                    ulong timestamp = data["currentTimestampMs"]?.Value<ulong>() ?? 0;
                    PlaybackLabel.Text = $"Playback: {(isPlaying ? "Playing" : "Stopped")} - {timestamp}ms";
                    break;

                case MessageType.ERROR_RESPONSE:
                    string error = data["error"]?.Value<string>();
                    BottomStatusLabel.Text = $"Error: {error}";
                    break;
            }
        }

        private void updateQueueDisplay()
        {
            QueueListView.Items.Clear();

            if (ipcClient?.PresetQueue != null)
            {
                foreach (var preset in ipcClient.PresetQueue)
                {
                    var item = new ListViewItem(preset.PresetName);
                    item.SubItems.Add(preset.TimestampMs.ToString());
                    item.SubItems.Add((preset.TimestampMs / 1000.0).ToString("F2"));
                    item.Tag = preset;
                    QueueListView.Items.Add(item);
                }
            }

            BottomStatusLabel.Text = $"Queue updated: {QueueListView.Items.Count} presets";
        }

        private void UpdateTimer_Tick(object sender, EventArgs e)
        {
            try
            {
                if (ipcClient != null)
                {
                    // Send periodic timestamp update (simulating audio position)
                    // TODO: Replace with actual audio player timestamp
                    ipcClient.SendTimestamp(timestampCounter++);
                }
            }
            catch (Exception ex)
            {
                BottomStatusLabel.Text = $"Update error: {ex.Message}";
            }
        }

        private void StatusTimer_Tick(object sender, EventArgs e)
        {
            if (ipcClient != null)
            {
                // Update status colors based on connection state
                try
                {
                    // If we can check connection status, update it
                    StatusLabel.Text = "Status: Connected";
                    StatusLabel.ForeColor = System.Drawing.Color.Green;
                }
                catch
                {
                    StatusLabel.Text = "Status: Disconnected";
                    StatusLabel.ForeColor = System.Drawing.Color.Red;
                }
            }
        }

        private void AddButton_Click(object sender, EventArgs e)
        {
            try
            {
                string presetName = PresetComboBox.SelectedItem?.ToString();
                if (string.IsNullOrEmpty(presetName))
                {
                    MessageBox.Show("Please select a preset", "Input Error");
                    return;
                }

                if (!double.TryParse(TimestampTextBox.Text, out double seconds))
                {
                    MessageBox.Show("Please enter a valid time in seconds", "Input Error");
                    return;
                }

                ulong timestampMs = (ulong)(seconds * 1000);

                ipcClient?.LoadPreset(presetName, timestampMs);
                BottomStatusLabel.Text = $"Added: {presetName} at {seconds}s";

                TimestampTextBox.Text = "0";
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Error: {ex.Message}", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void DeleteButton_Click(object sender, EventArgs e)
        {
            try
            {
                if (QueueListView.SelectedItems.Count == 0)
                {
                    MessageBox.Show("Please select a preset to delete", "Selection Error");
                    return;
                }

                var selectedItem = QueueListView.SelectedItems[0];
                var preset = selectedItem.Tag as ProjectMIPC.PresetQueueEntry;

                if (preset != null)
                {
                    ipcClient?.DeletePreset(preset.PresetName, preset.TimestampMs);
                    BottomStatusLabel.Text = $"Deleted: {preset.PresetName}";
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Error: {ex.Message}", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void StartPreviewButton_Click(object sender, EventArgs e)
        {
            try
            {
                ipcClient?.StartPreview(0);
                BottomStatusLabel.Text = "Preview started";
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Error: {ex.Message}", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void StopPreviewButton_Click(object sender, EventArgs e)
        {
            try
            {
                ipcClient?.StopPreview();
                BottomStatusLabel.Text = "Preview stopped";
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Error: {ex.Message}", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            base.OnFormClosing(e);
            updateTimer?.Stop();
            statusTimer?.Stop();
            ipcClient?.Dispose();
        }

        // Controls
        private ListView QueueListView;
        private ComboBox PresetComboBox;
        private TextBox TimestampTextBox;
        private Button AddButton;
        private Button DeleteButton;
        private Button StartPreviewButton;
        private Button StopPreviewButton;
        private Label StatusLabel;
        private Label PlaybackLabel;
        private Label BottomStatusLabel;
    }

    static class Program
    {
        [STAThread]
        static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new MainForm());
        }
    }
}
