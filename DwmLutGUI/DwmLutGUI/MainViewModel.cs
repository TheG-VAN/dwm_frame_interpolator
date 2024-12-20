﻿using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Windows.Input;
using System.Windows.Shapes;
using System.Xml;
using System.Xml.Linq;

namespace DwmLutGUI
{
    internal class MainViewModel : INotifyPropertyChanged
    {
        public event PropertyChangedEventHandler PropertyChanged;

        private string _activeText;
        private MonitorData _selectedMonitor;
        private bool _isActive;
        private Key _toggleKey;

        private readonly string _configPath;

        private bool _configChanged;
        private XElement _lastConfig;
        private XElement _activeConfig;

        public MainViewModel()
        {
            UpdateActiveStatus();
            var dispatcherTimer = new System.Windows.Threading.DispatcherTimer();
            dispatcherTimer.Tick += DispatcherTimer_Tick;
            dispatcherTimer.Interval = new TimeSpan(0, 0, 1);
            dispatcherTimer.Start();

            _configPath = AppDomain.CurrentDomain.BaseDirectory + "config.xml";

            _allMonitors = new List<MonitorData>();
            Monitors = new ObservableCollection<MonitorData>();
            UpdateMonitors();

            CanApply = !Injector.NoDebug;
            MonitorData.StaticPropertyChanged += MonitorDataOnStaticPropertyChanged;
        }

        private void MonitorDataOnStaticPropertyChanged(object sender, PropertyChangedEventArgs e)
        {
            OnPropertyChanged(nameof(SdrLutPath));
            OnPropertyChanged(nameof(HdrLutPath));
            SaveConfig();
        }

        public string ActiveText
        {
            private set
            {
                if (value == _activeText) return;
                _activeText = value;
                OnPropertyChanged();
            }
            get => _activeText;
        }

        public MonitorData SelectedMonitor
        {
            set
            {
                if (value == _selectedMonitor) return;
                _selectedMonitor = value;
                OnPropertyChanged();
                OnPropertyChanged(nameof(SdrLutPath));
                OnPropertyChanged(nameof(HdrLutPath));
                SaveConfig();
                string registryKeyPath = @"Software\DwmFrameInterpolator";
                string leftKeyName = "MonitorLeft";
                string topKeyName = "MonitorTop";
                using (RegistryKey key = Registry.LocalMachine.CreateSubKey(registryKeyPath))
                {
                    if (key != null)
                    {
                        int left = int.Parse(value.Position.Split(',')[0]);
                        int top = int.Parse(value.Position.Split(',')[1]);
                        key.SetValue(leftKeyName, left);
                        key.SetValue(topKeyName, top);
                    }
                }
            }
            get => _selectedMonitor;
        }

        private void UpdateConfigChanged()
        {
            _configChanged = _lastConfig != _activeConfig && !XNode.DeepEquals(_lastConfig, _activeConfig);
        }

        private void SaveConfig()
        {
            var xElem = new XElement("monitors",
                new XAttribute("lut_toggle", _toggleKey),
                new XAttribute("selected_monitor", _selectedMonitor.Name),
                _allMonitors.Select(x =>
                    new XElement("monitor", new XAttribute("path", x.DevicePath),
                        x.SdrLutPath != null ? new XAttribute("sdr_lut", x.SdrLutPath) : null,
                        x.HdrLutPath != null ? new XAttribute("hdr_lut", x.HdrLutPath) : null,
                        x.SdrLuts != null ? new XElement("sdr_luts", x.SdrLuts.Select(s => new XElement("sdr_lut", s))) : null)));

            xElem.Save(_configPath);

            _lastConfig = xElem;
            UpdateConfigChanged();
            UpdateActiveStatus();
        }

        public string SdrLutPath
        {
            set
            {
                if (SelectedMonitor == null || SelectedMonitor.SdrLutPath == value) return;
                SelectedMonitor.SdrLutPath = value;
                OnPropertyChanged();

                SaveConfig();
            }
            get => SelectedMonitor?.SdrLutPath;
        }

        public string HdrLutPath
        {
            set
            {
                if (SelectedMonitor == null || SelectedMonitor.HdrLutPath == value) return;
                SelectedMonitor.HdrLutPath = value;
                OnPropertyChanged();

                SaveConfig();
            }
            get => SelectedMonitor?.HdrLutPath;
        }

        public Key ToggleKey
        {
            set
            {
                if (value == _toggleKey) return;
                _toggleKey = value;
                OnPropertyChanged();
                SaveConfig();
            }
            get => _toggleKey;
        }

        public bool IsActive
        {
            set
            {
                if (value == _isActive) return;
                _isActive = value;
                OnPropertyChanged();
            }
            get => _isActive;
        }

        public bool CanApply { get; }

        private List<MonitorData> _allMonitors { get; }
        public ObservableCollection<MonitorData> Monitors { get; }

        public void UpdateMonitors()
        {
            var selectedPath = SelectedMonitor?.DevicePath;
            _allMonitors.Clear();
            Monitors.Clear();
            List<XElement> config = null;
            string monitorName = null;
            if (File.Exists(_configPath))
            {
                config = XElement.Load(_configPath).Descendants("monitor").ToList();
                try
                {
                    _toggleKey = (Key)Enum.Parse(typeof(Key), (string)XElement.Load(_configPath).Attribute("lut_toggle"));
                    monitorName = (string)XElement.Load(_configPath).Attribute("selected_monitor");
                }
                catch
                {
                    _toggleKey = Key.Pause;
                }
            }
            else
            {
                _toggleKey = Key.Pause;
            }

            var paths = WindowsDisplayAPI.DisplayConfig.PathInfo.GetActivePaths();
            foreach (var path in paths)
            {
                if (path.IsCloneMember) continue;
                var targetInfo = path.TargetsInfo[0];
                var deviceId = targetInfo.DisplayTarget.TargetId;
                var devicePath = targetInfo.DisplayTarget.DevicePath;
                var name = targetInfo.DisplayTarget.FriendlyName;
                if (string.IsNullOrEmpty(name))
                {
                    name = "???";
                }

                var connector = targetInfo.OutputTechnology.ToString();
                if (connector == "DisplayPortExternal")
                {
                    connector = "DisplayPort";
                }

                var position = path.Position.X + "," + path.Position.Y;

                string sdrLutPath = null;
                string hdrLutPath = null;
                var monitor = new MonitorData(devicePath, path.DisplaySource.SourceId + 1, name, connector, position,
                    sdrLutPath, hdrLutPath);
                _allMonitors.Add(monitor);
                Monitors.Add(monitor);
                if (name == monitorName)
                {
                    SelectedMonitor = monitor;
                }
            }
            if (SelectedMonitor == null)
            {
                SelectedMonitor = Monitors.First();
            }
        }

        public void ReInject()
        {
            Injector.Uninject();
            //if (!Monitors.All(monitor =>
            //        string.IsNullOrEmpty(monitor.SdrLutPath) && string.IsNullOrEmpty(monitor.HdrLutPath)))
            //{
            Injector.Inject(Monitors);
            //}

            _activeConfig = _lastConfig;
            UpdateConfigChanged();

            UpdateActiveStatus();
        }

        public void Uninject()
        {
            Injector.Uninject();
            UpdateActiveStatus();
        }

        private void UpdateActiveStatus()
        {
            var status = Injector.GetStatus();
            if (status != null)
            {
                IsActive = (bool)status;
                if (status == true)
                {
                    ActiveText = "Active" + (_configChanged ? " (changed)" : "");
                }
                else
                {
                    ActiveText = "Inactive";
                }
            }
            else
            {
                IsActive = false;
                ActiveText = "???";
            }
        }

        public void OnDisplaySettingsChanged(object sender, EventArgs e)
        {
            UpdateMonitors();
            if (!_configChanged)
            {
                ReInject();
            }
        }

        private void DispatcherTimer_Tick(object sender, EventArgs e)
        {
            UpdateActiveStatus();
        }

        private void OnPropertyChanged([CallerMemberName] string name = null)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
        }
    }
}