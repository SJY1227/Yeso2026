# Native USB CDC does not need UART baud configuration. SerialPort.Open() changes
# Windows modem lines and can reset the S3; this console leaves those lines alone.
if (-not ('RoutineDeviceConsole' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;
using Microsoft.Win32.SafeHandles;
public sealed class RoutineDeviceConsole : IDisposable {
    [StructLayout(LayoutKind.Sequential)]
    struct Timeouts { public uint Interval, ReadMultiplier, ReadConstant, WriteMultiplier, WriteConstant; }
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    static extern SafeFileHandle CreateFile(string name, uint access, uint share, IntPtr security, uint creation, uint flags, IntPtr template);
    [DllImport("kernel32.dll", SetLastError=true)]
    static extern bool SetCommTimeouts(SafeFileHandle file, ref Timeouts timeouts);
    [DllImport("kernel32.dll", SetLastError=true)]
    static extern bool ReadFile(SafeFileHandle file, byte[] data, uint length, out uint read, IntPtr overlapped);
    [DllImport("kernel32.dll", SetLastError=true)]
    static extern bool WriteFile(SafeFileHandle file, byte[] data, uint length, out uint written, IntPtr overlapped);
    SafeFileHandle handle;
    public RoutineDeviceConsole(string port) {
        if (!System.Text.RegularExpressions.Regex.IsMatch(port, @"^COM[0-9]+$")) throw new ArgumentException("Expected a COM port");
        handle = CreateFile(@"\\.\" + port, 0xc0000000, 0, IntPtr.Zero, 3, 0, IntPtr.Zero);
        if (handle.IsInvalid) { int error = Marshal.GetLastWin32Error(); handle.Dispose(); throw new Win32Exception(error); }
        // Keep a short pending read when empty rather than spinning with a
        // zero-wait read. This leaves modem control lines unchanged.
        var timeouts = new Timeouts { Interval = uint.MaxValue, ReadMultiplier = uint.MaxValue, ReadConstant = 20, WriteConstant = 2000 };
        if (!SetCommTimeouts(handle, ref timeouts)) { int error = Marshal.GetLastWin32Error(); handle.Dispose(); throw new Win32Exception(error); }
    }
    public string ReadExisting() {
        var bytes = new byte[8192]; uint read;
        if (!ReadFile(handle, bytes, (uint)bytes.Length, out read, IntPtr.Zero)) throw new Win32Exception(Marshal.GetLastWin32Error());
        return Encoding.UTF8.GetString(bytes, 0, (int)read);
    }
    public void WriteLine(string text) {
        WriteBytes(Encoding.ASCII.GetBytes(text + "\n"));
    }
    public void WriteBytes(byte[] bytes) {
        uint written;
        if (!WriteFile(handle, bytes, (uint)bytes.Length, out written, IntPtr.Zero)) throw new Win32Exception(Marshal.GetLastWin32Error());
        if (written != bytes.Length) throw new InvalidOperationException("Incomplete USB write");
    }
    public void Close() { Dispose(); }
    public void Dispose() { if (handle != null) { handle.Dispose(); handle = null; } }
}
'@
}
