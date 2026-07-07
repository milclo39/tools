using System;
using System.Windows;
using UvcExtensionControlApp.ViewModels;

namespace UvcExtensionControlApp
{
    public partial class MainWindow : Window
    {
        private readonly MainViewModel _viewModel;

        public MainWindow()
        {
            InitializeComponent();
            _viewModel = new MainViewModel();
            DataContext = _viewModel;
            Closed += OnClosed;
        }

        private void OnClosed(object sender, EventArgs e)
        {
            _viewModel.Dispose();
        }
    }
}
