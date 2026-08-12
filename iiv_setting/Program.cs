using System;
using System.Diagnostics;
using System.Linq;
using System.Net;
using System.Windows.Forms;

namespace iiv_setting
{
    internal static class Program
    {
        /// <summary>
        /// Decoded `--return-cmd` value (if provided).
        /// Example final form: C:\local\msys2\home\...\iiv_mon.exe
        /// </summary>
        internal static string? ReturnCmd { get; private set; }

        /// <summary>
        ///  The main entry point for the application.
        /// </summary>
        [STAThread]
        static void Main(string[] args)
        {
            // To customize application configuration such as set high DPI settings or default font,
            // see https://aka.ms/applicationconfiguration.
            ApplicationConfiguration.Initialize();

            ParseReturnCmd(args);

            if (string.IsNullOrEmpty(ReturnCmd))
            {
                MessageBox.Show(
                    "No --return-cmd provided. The application will continue without it.",
                    Application.ProductName,
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Warning);
            }
            Debug.WriteLine($"Parsed --return-cmd: {ReturnCmd}");
            
            Application.Run(new FormSetting());

            if(!string.IsNullOrEmpty(ReturnCmd))
            {
                try
                {
                    Process.Start(ReturnCmd);
                }
                catch (Exception ex)
                {
                    MessageBox.Show(
                        $"Failed to execute --return-cmd: {ReturnCmd}\n\nError: {ex.Message}",
                        Application.ProductName,
                        MessageBoxButtons.OK,
                        MessageBoxIcon.Error);
                }
            }
        }

        private static void ParseReturnCmd(string[]? args)
        {
            if (args == null || args.Length == 0)
                return;

            const string key = "--return-cmd";

            for (int i = 0; i < args.Length; i++)
            {
                var arg = args[i] ?? string.Empty;

                if (string.Equals(arg, key, StringComparison.OrdinalIgnoreCase))
                {
                    // Next token is the value, if present.
                    if (i + 1 < args.Length)
                    {
                        var raw = args[i + 1] ?? string.Empty;
                        ReturnCmd = DecodeAndClean(raw);
                        return;
                    }
                }

                if (arg.StartsWith(key + "=", StringComparison.OrdinalIgnoreCase))
                {
                    var raw = arg.Substring(key.Length + 1);
                    ReturnCmd = DecodeAndClean(raw);
                    return;
                }

                if (arg.StartsWith(key + " ", StringComparison.OrdinalIgnoreCase))
                {
                    var raw = arg.Substring(key.Length + 1).Trim();
                    ReturnCmd = DecodeAndClean(raw);
                    return;
                }

                if (arg.StartsWith(key, StringComparison.OrdinalIgnoreCase) && arg.Length > key.Length)
                {
                    // Covers cases like: "--return-cmd%20... " or "--return-cmd%3D..."
                    var raw = arg.Substring(key.Length).TrimStart('=', ' ');
                    if (!string.IsNullOrEmpty(raw))
                    {
                        ReturnCmd = DecodeAndClean(raw);
                        return;
                    }
                }
            }
        }

        private static string? DecodeAndClean(string raw)
        {
            if (string.IsNullOrWhiteSpace(raw))
                return null;

            // URL-decode (handles %22 -> " etc).
            var decoded = WebUtility.UrlDecode(raw);

            if (decoded == null)
                return null;

            return decoded;
        }
    }
}