//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

// The Windows APIs used in this file will fail WACK tests.
// remove #define _APP_PERFORMANCE_  in the final build
//#define _APP_PERFORMANCE_

using System;
using System.Diagnostics;
using System.Runtime.InteropServices;

namespace PeerConnectionClient.Utilities
{
#if _APP_PERFORMANCE_
    static class MEMData
    {
    // Todo: added by ling, WINDOWS_PHONE_APP is not available for vs2015 when build win10 for arm
    // check USE_WIN10_PHONE_DLL (defined by us) until vs2015 fix this
#if WINDOWS_PHONE_APP || USE_WIN10_PHONE_DLL
        [DllImport("api-ms-win-core-sysinfo-l1-2-0.dll", CharSet = CharSet.Unicode, ExactSpelling = true, PreserveSig = true)]
        private static extern IntPtr GetCurrentProcess();
#else
        [DllImport("kernel32.dll", ExactSpelling = true)]
        private static extern IntPtr GetCurrentProcess();
#endif
#if WINDOWS_PHONE_APP || USE_WIN10_PHONE_DLL
        [DllImport("api-ms-win-core-sysinfo-l1-2-0.dll", CharSet = CharSet.Unicode, ExactSpelling = true, PreserveSig = true)]
        private static extern bool GetProcessMemoryInfo(IntPtr hProcess, out PROCESS_MEMORY_COUNTERS_EX counters, uint size);
#else
        [DllImport("psapi.dll", ExactSpelling = true, SetLastError = true)]
        private static extern bool GetProcessMemoryInfo(IntPtr hProcess, out PROCESS_MEMORY_COUNTERS_EX counters, uint size);
#endif


        /// <summary>
        /// Get the current memory usage
        /// </summary>
        public static Int64 GetMEMUsage()
        {
            Int64 ret = 0;

            PROCESS_MEMORY_COUNTERS_EX memoryCounters;

            memoryCounters.cb = (uint)System.Runtime.InteropServices.Marshal.SizeOf<PROCESS_MEMORY_COUNTERS_EX>();

            if (GetProcessMemoryInfo(GetCurrentProcess(), out memoryCounters, memoryCounters.cb))
            {
                ret = memoryCounters.PrivateUsage.ToInt64();

            }
            Debug.WriteLine(String.Format("Memory usage:{0}", ret));

            return ret;
        }

    }

    static class CPUData
    {
        [StructLayout(LayoutKind.Sequential)]
        private struct FileTime
        {
            public UInt32 Low;
            public UInt32 High;
        }

        /// <summary>
        /// Uitility function to convert FileTime to uint64
        /// </summary>
        private static UInt64 ToUInt64(FileTime time)
        {
            return ((UInt64)time.High << 32) + time.Low;
        }

#if WINDOWS_PHONE_APP || USE_WIN10_PHONE_DLL
        [DllImport("api-ms-win-core-sysinfo-l1-2-0.dll", CharSet = CharSet.Unicode, ExactSpelling = true, PreserveSig = true)]
        private static extern IntPtr GetCurrentProcess();
#else
        [DllImport("kernel32.dll", ExactSpelling = true)]
        private static extern IntPtr GetCurrentProcess();
#endif

#if WINDOWS_PHONE_APP || USE_WIN10_PHONE_DLL
        [DllImport("api-ms-win-core-sysinfo-l1-2-0.dll", CharSet = CharSet.Unicode, ExactSpelling = true, PreserveSig = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool GetProcessTimes(
            IntPtr hProcess,
            out FileTime lpCreationTime,
            out FileTime lpExitTime,
            out FileTime lpKernelTime,
            out FileTime lpUserTime);
#else
        [DllImport("kernel32.dll", ExactSpelling = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool GetProcessTimes(
            IntPtr hProcess,
            out FileTime lpCreationTime,
            out FileTime lpExitTime,
            out FileTime lpKernelTime,
            out FileTime lpUserTime);
#endif


        public struct ProcessTimes
        {
            public UInt64 CreationTime;
            public UInt64 ExitTime;
            public UInt64 KernelTime;
            public UInt64 UserTime;
        }

        /// <summary>
        /// Get the cpu time for this process
        /// </summary>
        public static ProcessTimes GetProcessTimes()
        {
            FileTime creation, exit, kernel, user;

            if (!GetProcessTimes(GetCurrentProcess(),
                    out creation, out exit, out kernel, out user))
                throw new Exception(":'(");

            return new ProcessTimes
            {
                CreationTime = ToUInt64(creation),
                ExitTime = ToUInt64(exit),
                KernelTime = ToUInt64(kernel),
                UserTime = ToUInt64(user)
            };
        }

        /// <summary>
        /// Calculate CPU usage, e.g.: this process time vs system process time
        /// Return the CPU usage in percentage
        /// </summary>
        public static double GetCPUUsage() 
        {

            double ret = 0.0;

            // retrieve process time
            ProcessTimes processTimes = GetProcessTimes();

            UInt64 currentProcessTime = processTimes.KernelTime + processTimes.UserTime;

            // retrieve system time
            // get number of CPU cores, then, check system time for every CPU core
            if(numberOfProcessors == 0 ) {
                SystemInfo info;
                GetSystemInfo(out info);
                numberOfProcessors = info.NumberOfProcessors;
            }

            int size = System.Runtime.InteropServices.Marshal.SizeOf<SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION>();

            size = (int) (size * numberOfProcessors);

            IntPtr systemProcessInfoBuff = Marshal.AllocHGlobal(size); // should be more than adequate
            uint length = 0;
            NtStatus result = NtQuerySystemInformation(SYSTEM_INFORMATION_CLASS.SystemProcessorPerformanceInformation, systemProcessInfoBuff, (uint)size, out length);

            if (result != NtStatus.Success)
            {
                Debug.WriteLine(String.Format("[Error] Failed to obtain processor performance info ({0}", result));
                return ret;
            }
            UInt64 currentSystemTime = 0;
            var systemProcInfoData = systemProcessInfoBuff;
            for (int i = 0; i < numberOfProcessors; i++)
            {
                SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION processPerInfo = (SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION)Marshal.PtrToStructure<SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION>(systemProcInfoData);

                currentSystemTime += processPerInfo.KernelTime + processPerInfo.UserTime;
            }

            // we need to at least measure twice 
            if (previousProcessTime != 0 && previousSystemTIme != 0)
            {

                ret = ((double)(currentProcessTime - previousProcessTime) / (double)(currentSystemTime - previousSystemTIme)) * 100.0;
            }

            previousProcessTime = currentProcessTime;
            previousSystemTIme = currentSystemTime;
            Debug.WriteLine(String.Format("CPU usage:{0}%", ret));
            return ret;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION
        {
            public UInt64 IdleTime;
            public UInt64 KernelTime;
            public UInt64 UserTime;
            public UInt64 Reserved10;
            public UInt64 Reserved11;
            public ulong Reserved2;
        }

        /// <summary>Retrieves the specified system information.</summary>
        /// <param name="InfoClass">indicate the kind of system information to be retrieved</param>
        /// <param name="Info">a buffer that receives the requested information</param>
        /// <param name="Size">The allocation size of the buffer pointed to by Info</param>
        /// <param name="Length">If null, ignored.  Otherwise tells you the size of the information returned by the kernel.</param>
        /// <returns>Status Information</returns>
        /// http://msdn.microsoft.com/en-us/library/windows/desktop/ms724509%28v=vs.85%29.aspx
        [DllImport("ntdll.dll", SetLastError = false, ExactSpelling = true)]
        private static extern NtStatus NtQuerySystemInformation(SYSTEM_INFORMATION_CLASS InfoClass, IntPtr Info, UInt32 Size, out UInt32 Length);


#if WINDOWS_PHONE_APP || USE_WIN10_PHONE_DLL
        [DllImport("api-ms-win-core-sysinfo-l1-2-0.dll", CharSet = CharSet.Unicode, ExactSpelling = true, PreserveSig = true)]
        private static extern void GetSystemInfo(out SystemInfo Info);
#else
        [DllImport("kernel32.dll", ExactSpelling = true, SetLastError = false)]

        private static extern void GetSystemInfo(out SystemInfo Info);
#endif


        private static uint numberOfProcessors = 0;
        private static UInt64 previousProcessTime = 0;
        private static UInt64 previousSystemTIme = 0;

    }

    /// The code below was copied from http://pinvoke.net
    /// reviewers can ignore them


    /// <summary>
    /// A NT status value.
    /// </summary>
    public enum NtStatus : uint
    {
        // Success
        Success = 0x00000000,
        Wait0 = 0x00000000,
        Wait1 = 0x00000001,
        Wait2 = 0x00000002,
        Wait3 = 0x00000003,
        Wait63 = 0x0000003f,
        Abandoned = 0x00000080,
        AbandonedWait0 = 0x00000080,
        AbandonedWait1 = 0x00000081,
        AbandonedWait2 = 0x00000082,
        AbandonedWait3 = 0x00000083,
        AbandonedWait63 = 0x000000bf,
        UserApc = 0x000000c0,
        KernelApc = 0x00000100,
        Alerted = 0x00000101,
        Timeout = 0x00000102,
        Pending = 0x00000103,
        Reparse = 0x00000104,
        MoreEntries = 0x00000105,
        NotAllAssigned = 0x00000106,
        SomeNotMapped = 0x00000107,
        OpLockBreakInProgress = 0x00000108,
        VolumeMounted = 0x00000109,
        RxActCommitted = 0x0000010a,
        NotifyCleanup = 0x0000010b,
        NotifyEnumDir = 0x0000010c,
        NoQuotasForAccount = 0x0000010d,
        PrimaryTransportConnectFailed = 0x0000010e,
        PageFaultTransition = 0x00000110,
        PageFaultDemandZero = 0x00000111,
        PageFaultCopyOnWrite = 0x00000112,
        PageFaultGuardPage = 0x00000113,
        PageFaultPagingFile = 0x00000114,
        CrashDump = 0x00000116,
        ReparseObject = 0x00000118,
        NothingToTerminate = 0x00000122,
        ProcessNotInJob = 0x00000123,
        ProcessInJob = 0x00000124,
        ProcessCloned = 0x00000129,
        FileLockedWithOnlyReaders = 0x0000012a,
        FileLockedWithWriters = 0x0000012b,

        // Informational
        Informational = 0x40000000,
        ObjectNameExists = 0x40000000,
        ThreadWasSuspended = 0x40000001,
        WorkingSetLimitRange = 0x40000002,
        ImageNotAtBase = 0x40000003,
        RegistryRecovered = 0x40000009,

        // Warning
        Warning = 0x80000000,
        GuardPageViolation = 0x80000001,
        DatatypeMisalignment = 0x80000002,
        Breakpoint = 0x80000003,
        SingleStep = 0x80000004,
        BufferOverflow = 0x80000005,
        NoMoreFiles = 0x80000006,
        HandlesClosed = 0x8000000a,
        PartialCopy = 0x8000000d,
        DeviceBusy = 0x80000011,
        InvalidEaName = 0x80000013,
        EaListInconsistent = 0x80000014,
        NoMoreEntries = 0x8000001a,
        LongJump = 0x80000026,
        DllMightBeInsecure = 0x8000002b,

        // Error
        Error = 0xc0000000,
        Unsuccessful = 0xc0000001,
        NotImplemented = 0xc0000002,
        InvalidInfoClass = 0xc0000003,
        InfoLengthMismatch = 0xc0000004,
        AccessViolation = 0xc0000005,
        InPageError = 0xc0000006,
        PagefileQuota = 0xc0000007,
        InvalidHandle = 0xc0000008,
        BadInitialStack = 0xc0000009,
        BadInitialPc = 0xc000000a,
        InvalidCid = 0xc000000b,
        TimerNotCanceled = 0xc000000c,
        InvalidParameter = 0xc000000d,
        NoSuchDevice = 0xc000000e,
        NoSuchFile = 0xc000000f,
        InvalidDeviceRequest = 0xc0000010,
        EndOfFile = 0xc0000011,
        WrongVolume = 0xc0000012,
        NoMediaInDevice = 0xc0000013,
        NoMemory = 0xc0000017,
        NotMappedView = 0xc0000019,
        UnableToFreeVm = 0xc000001a,
        UnableToDeleteSection = 0xc000001b,
        IllegalInstruction = 0xc000001d,
        AlreadyCommitted = 0xc0000021,
        AccessDenied = 0xc0000022,
        BufferTooSmall = 0xc0000023,
        ObjectTypeMismatch = 0xc0000024,
        NonContinuableException = 0xc0000025,
        BadStack = 0xc0000028,
        NotLocked = 0xc000002a,
        NotCommitted = 0xc000002d,
        InvalidParameterMix = 0xc0000030,
        ObjectNameInvalid = 0xc0000033,
        ObjectNameNotFound = 0xc0000034,
        ObjectNameCollision = 0xc0000035,
        ObjectPathInvalid = 0xc0000039,
        ObjectPathNotFound = 0xc000003a,
        ObjectPathSyntaxBad = 0xc000003b,
        DataOverrun = 0xc000003c,
        DataLate = 0xc000003d,
        DataError = 0xc000003e,
        CrcError = 0xc000003f,
        SectionTooBig = 0xc0000040,
        PortConnectionRefused = 0xc0000041,
        InvalidPortHandle = 0xc0000042,
        SharingViolation = 0xc0000043,
        QuotaExceeded = 0xc0000044,
        InvalidPageProtection = 0xc0000045,
        MutantNotOwned = 0xc0000046,
        SemaphoreLimitExceeded = 0xc0000047,
        PortAlreadySet = 0xc0000048,
        SectionNotImage = 0xc0000049,
        SuspendCountExceeded = 0xc000004a,
        ThreadIsTerminating = 0xc000004b,
        BadWorkingSetLimit = 0xc000004c,
        IncompatibleFileMap = 0xc000004d,
        SectionProtection = 0xc000004e,
        EasNotSupported = 0xc000004f,
        EaTooLarge = 0xc0000050,
        NonExistentEaEntry = 0xc0000051,
        NoEasOnFile = 0xc0000052,
        EaCorruptError = 0xc0000053,
        FileLockConflict = 0xc0000054,
        LockNotGranted = 0xc0000055,
        DeletePending = 0xc0000056,
        CtlFileNotSupported = 0xc0000057,
        UnknownRevision = 0xc0000058,
        RevisionMismatch = 0xc0000059,
        InvalidOwner = 0xc000005a,
        InvalidPrimaryGroup = 0xc000005b,
        NoImpersonationToken = 0xc000005c,
        CantDisableMandatory = 0xc000005d,
        NoLogonServers = 0xc000005e,
        NoSuchLogonSession = 0xc000005f,
        NoSuchPrivilege = 0xc0000060,
        PrivilegeNotHeld = 0xc0000061,
        InvalidAccountName = 0xc0000062,
        UserExists = 0xc0000063,
        NoSuchUser = 0xc0000064,
        GroupExists = 0xc0000065,
        NoSuchGroup = 0xc0000066,
        MemberInGroup = 0xc0000067,
        MemberNotInGroup = 0xc0000068,
        LastAdmin = 0xc0000069,
        WrongPassword = 0xc000006a,
        IllFormedPassword = 0xc000006b,
        PasswordRestriction = 0xc000006c,
        LogonFailure = 0xc000006d,
        AccountRestriction = 0xc000006e,
        InvalidLogonHours = 0xc000006f,
        InvalidWorkstation = 0xc0000070,
        PasswordExpired = 0xc0000071,
        AccountDisabled = 0xc0000072,
        NoneMapped = 0xc0000073,
        TooManyLuidsRequested = 0xc0000074,
        LuidsExhausted = 0xc0000075,
        InvalidSubAuthority = 0xc0000076,
        InvalidAcl = 0xc0000077,
        InvalidSid = 0xc0000078,
        InvalidSecurityDescr = 0xc0000079,
        ProcedureNotFound = 0xc000007a,
        InvalidImageFormat = 0xc000007b,
        NoToken = 0xc000007c,
        BadInheritanceAcl = 0xc000007d,
        RangeNotLocked = 0xc000007e,
        DiskFull = 0xc000007f,
        ServerDisabled = 0xc0000080,
        ServerNotDisabled = 0xc0000081,
        TooManyGuidsRequested = 0xc0000082,
        GuidsExhausted = 0xc0000083,
        InvalidIdAuthority = 0xc0000084,
        AgentsExhausted = 0xc0000085,
        InvalidVolumeLabel = 0xc0000086,
        SectionNotExtended = 0xc0000087,
        NotMappedData = 0xc0000088,
        ResourceDataNotFound = 0xc0000089,
        ResourceTypeNotFound = 0xc000008a,
        ResourceNameNotFound = 0xc000008b,
        ArrayBoundsExceeded = 0xc000008c,
        FloatDenormalOperand = 0xc000008d,
        FloatDivideByZero = 0xc000008e,
        FloatInexactResult = 0xc000008f,
        FloatInvalidOperation = 0xc0000090,
        FloatOverflow = 0xc0000091,
        FloatStackCheck = 0xc0000092,
        FloatUnderflow = 0xc0000093,
        IntegerDivideByZero = 0xc0000094,
        IntegerOverflow = 0xc0000095,
        PrivilegedInstruction = 0xc0000096,
        TooManyPagingFiles = 0xc0000097,
        FileInvalid = 0xc0000098,
        InstanceNotAvailable = 0xc00000ab,
        PipeNotAvailable = 0xc00000ac,
        InvalidPipeState = 0xc00000ad,
        PipeBusy = 0xc00000ae,
        IllegalFunction = 0xc00000af,
        PipeDisconnected = 0xc00000b0,
        PipeClosing = 0xc00000b1,
        PipeConnected = 0xc00000b2,
        PipeListening = 0xc00000b3,
        InvalidReadMode = 0xc00000b4,
        IoTimeout = 0xc00000b5,
        FileForcedClosed = 0xc00000b6,
        ProfilingNotStarted = 0xc00000b7,
        ProfilingNotStopped = 0xc00000b8,
        NotSameDevice = 0xc00000d4,
        FileRenamed = 0xc00000d5,
        CantWait = 0xc00000d8,
        PipeEmpty = 0xc00000d9,
        CantTerminateSelf = 0xc00000db,
        InternalError = 0xc00000e5,
        InvalidParameter1 = 0xc00000ef,
        InvalidParameter2 = 0xc00000f0,
        InvalidParameter3 = 0xc00000f1,
        InvalidParameter4 = 0xc00000f2,
        InvalidParameter5 = 0xc00000f3,
        InvalidParameter6 = 0xc00000f4,
        InvalidParameter7 = 0xc00000f5,
        InvalidParameter8 = 0xc00000f6,
        InvalidParameter9 = 0xc00000f7,
        InvalidParameter10 = 0xc00000f8,
        InvalidParameter11 = 0xc00000f9,
        InvalidParameter12 = 0xc00000fa,
        MappedFileSizeZero = 0xc000011e,
        TooManyOpenedFiles = 0xc000011f,
        Cancelled = 0xc0000120,
        CannotDelete = 0xc0000121,
        InvalidComputerName = 0xc0000122,
        FileDeleted = 0xc0000123,
        SpecialAccount = 0xc0000124,
        SpecialGroup = 0xc0000125,
        SpecialUser = 0xc0000126,
        MembersPrimaryGroup = 0xc0000127,
        FileClosed = 0xc0000128,
        TooManyThreads = 0xc0000129,
        ThreadNotInProcess = 0xc000012a,
        TokenAlreadyInUse = 0xc000012b,
        PagefileQuotaExceeded = 0xc000012c,
        CommitmentLimit = 0xc000012d,
        InvalidImageLeFormat = 0xc000012e,
        InvalidImageNotMz = 0xc000012f,
        InvalidImageProtect = 0xc0000130,
        InvalidImageWin16 = 0xc0000131,
        LogonServer = 0xc0000132,
        DifferenceAtDc = 0xc0000133,
        SynchronizationRequired = 0xc0000134,
        DllNotFound = 0xc0000135,
        IoPrivilegeFailed = 0xc0000137,
        OrdinalNotFound = 0xc0000138,
        EntryPointNotFound = 0xc0000139,
        ControlCExit = 0xc000013a,
        PortNotSet = 0xc0000353,
        DebuggerInactive = 0xc0000354,
        CallbackBypass = 0xc0000503,
        PortClosed = 0xc0000700,
        MessageLost = 0xc0000701,
        InvalidMessage = 0xc0000702,
        RequestCanceled = 0xc0000703,
        RecursiveDispatch = 0xc0000704,
        LpcReceiveBufferExpected = 0xc0000705,
        LpcInvalidConnectionUsage = 0xc0000706,
        LpcRequestsNotAllowed = 0xc0000707,
        ResourceInUse = 0xc0000708,
        ProcessIsProtected = 0xc0000712,
        VolumeDirty = 0xc0000806,
        FileCheckedOut = 0xc0000901,
        CheckOutRequired = 0xc0000902,
        BadFileType = 0xc0000903,
        FileTooLarge = 0xc0000904,
        FormsAuthRequired = 0xc0000905,
        VirusInfected = 0xc0000906,
        VirusDeleted = 0xc0000907,
        TransactionalConflict = 0xc0190001,
        InvalidTransaction = 0xc0190002,
        TransactionNotActive = 0xc0190003,
        TmInitializationFailed = 0xc0190004,
        RmNotActive = 0xc0190005,
        RmMetadataCorrupt = 0xc0190006,
        TransactionNotJoined = 0xc0190007,
        DirectoryNotRm = 0xc0190008,
        CouldNotResizeLog = 0xc0190009,
        TransactionsUnsupportedRemote = 0xc019000a,
        LogResizeInvalidSize = 0xc019000b,
        RemoteFileVersionMismatch = 0xc019000c,
        CrmProtocolAlreadyExists = 0xc019000f,
        TransactionPropagationFailed = 0xc0190010,
        CrmProtocolNotFound = 0xc0190011,
        TransactionSuperiorExists = 0xc0190012,
        TransactionRequestNotValid = 0xc0190013,
        TransactionNotRequested = 0xc0190014,
        TransactionAlreadyAborted = 0xc0190015,
        TransactionAlreadyCommitted = 0xc0190016,
        TransactionInvalidMarshallBuffer = 0xc0190017,
        CurrentTransactionNotValid = 0xc0190018,
        LogGrowthFailed = 0xc0190019,
        ObjectNoLongerExists = 0xc0190021,
        StreamMiniversionNotFound = 0xc0190022,
        StreamMiniversionNotValid = 0xc0190023,
        MiniversionInaccessibleFromSpecifiedTransaction = 0xc0190024,
        CantOpenMiniversionWithModifyIntent = 0xc0190025,
        CantCreateMoreStreamMiniversions = 0xc0190026,
        HandleNoLongerValid = 0xc0190028,
        NoTxfMetadata = 0xc0190029,
        LogCorruptionDetected = 0xc0190030,
        CantRecoverWithHandleOpen = 0xc0190031,
        RmDisconnected = 0xc0190032,
        EnlistmentNotSuperior = 0xc0190033,
        RecoveryNotNeeded = 0xc0190034,
        RmAlreadyStarted = 0xc0190035,
        FileIdentityNotPersistent = 0xc0190036,
        CantBreakTransactionalDependency = 0xc0190037,
        CantCrossRmBoundary = 0xc0190038,
        TxfDirNotEmpty = 0xc0190039,
        IndoubtTransactionsExist = 0xc019003a,
        TmVolatile = 0xc019003b,
        RollbackTimerExpired = 0xc019003c,
        TxfAttributeCorrupt = 0xc019003d,
        EfsNotAllowedInTransaction = 0xc019003e,
        TransactionalOpenNotAllowed = 0xc019003f,
        TransactedMappingUnsupportedRemote = 0xc0190040,
        TxfMetadataAlreadyPresent = 0xc0190041,
        TransactionScopeCallbacksNotSet = 0xc0190042,
        TransactionRequiredPromotion = 0xc0190043,
        CannotExecuteFileInTransaction = 0xc0190044,
        TransactionsNotFrozen = 0xc0190045,

        MaximumNtStatus = 0xffffffff
    }

    public enum SYSTEM_INFORMATION_CLASS
    {
        SystemBasicInformation = 0x0000,
        SystemProcessorInformation = 0x0001,
        SystemPerformanceInformation = 0x0002,
        SystemTimeOfDayInformation = 0x0003,
        SystemPathInformation = 0x0004,
        SystemProcessInformation = 0x0005,
        SystemCallCountInformation = 0x0006,
        SystemDeviceInformation = 0x0007,
        SystemProcessorPerformanceInformation = 0x0008,
        SystemFlagsInformation = 0x0009,
        SystemCallTimeInformation = 0x000A,
        SystemModuleInformation = 0x000B,
        SystemLocksInformation = 0x000C,
        SystemStackTraceInformation = 0x000D,
        SystemPagedPoolInformation = 0x000E,
        SystemNonPagedPoolInformation = 0x000F,
        SystemHandleInformation = 0x0010,
        SystemObjectInformation = 0x0011,
        SystemPageFileInformation = 0x0012,
        SystemVdmInstemulInformation = 0x0013,
        SystemVdmBopInformation = 0x0014,
        SystemFileCacheInformation = 0x0015,
        SystemPoolTagInformation = 0x0016,
        SystemInterruptInformation = 0x0017,
        SystemDpcBehaviorInformation = 0x0018,
        SystemFullMemoryInformation = 0x0019,
        SystemLoadGdiDriverInformation = 0x001A,
        SystemUnloadGdiDriverInformation = 0x001B,
        SystemTimeAdjustmentInformation = 0x001C,
        SystemSummaryMemoryInformation = 0x001D,
        SystemMirrorMemoryInformation = 0x001E,
        SystemPerformanceTraceInformation = 0x001F,
        SystemCrashDumpInformation = 0x0020,
        SystemExceptionInformation = 0x0021,
        SystemCrashDumpStateInformation = 0x0022,
        SystemKernelDebuggerInformation = 0x0023,
        SystemContextSwitchInformation = 0x0024,
        SystemRegistryQuotaInformation = 0x0025,
        SystemExtendServiceTableInformation = 0x0026,
        SystemPrioritySeperation = 0x0027,
        SystemVerifierAddDriverInformation = 0x0028,
        SystemVerifierRemoveDriverInformation = 0x0029,
        SystemProcessorIdleInformation = 0x002A,
        SystemLegacyDriverInformation = 0x002B,
        SystemCurrentTimeZoneInformation = 0x002C,
        SystemLookasideInformation = 0x002D,
        SystemTimeSlipNotification = 0x002E,
        SystemSessionCreate = 0x002F,
        SystemSessionDetach = 0x0030,
        SystemSessionInformation = 0x0031,
        SystemRangeStartInformation = 0x0032,
        SystemVerifierInformation = 0x0033,
        SystemVerifierThunkExtend = 0x0034,
        SystemSessionProcessInformation = 0x0035,
        SystemLoadGdiDriverInSystemSpace = 0x0036,
        SystemNumaProcessorMap = 0x0037,
        SystemPrefetcherInformation = 0x0038,
        SystemExtendedProcessInformation = 0x0039,
        SystemRecommendedSharedDataAlignment = 0x003A,
        SystemComPlusPackage = 0x003B,
        SystemNumaAvailableMemory = 0x003C,
        SystemProcessorPowerInformation = 0x003D,
        SystemEmulationBasicInformation = 0x003E,
        SystemEmulationProcessorInformation = 0x003F,
        SystemExtendedHandleInformation = 0x0040,
        SystemLostDelayedWriteInformation = 0x0041,
        SystemBigPoolInformation = 0x0042,
        SystemSessionPoolTagInformation = 0x0043,
        SystemSessionMappedViewInformation = 0x0044,
        SystemHotpatchInformation = 0x0045,
        SystemObjectSecurityMode = 0x0046,
        SystemWatchdogTimerHandler = 0x0047,
        SystemWatchdogTimerInformation = 0x0048,
        SystemLogicalProcessorInformation = 0x0049,
        SystemWow64SharedInformationObsolete = 0x004A,
        SystemRegisterFirmwareTableInformationHandler = 0x004B,
        SystemFirmwareTableInformation = 0x004C,
        SystemModuleInformationEx = 0x004D,
        SystemVerifierTriageInformation = 0x004E,
        SystemSuperfetchInformation = 0x004F,
        SystemMemoryListInformation = 0x0050, // SYSTEM_MEMORY_LIST_INFORMATION
        SystemFileCacheInformationEx = 0x0051,
        SystemThreadPriorityClientIdInformation = 0x0052,
        SystemProcessorIdleCycleTimeInformation = 0x0053,
        SystemVerifierCancellationInformation = 0x0054,
        SystemProcessorPowerInformationEx = 0x0055,
        SystemRefTraceInformation = 0x0056,
        SystemSpecialPoolInformation = 0x0057,
        SystemProcessIdInformation = 0x0058,
        SystemErrorPortInformation = 0x0059,
        SystemBootEnvironmentInformation = 0x005A,
        SystemHypervisorInformation = 0x005B,
        SystemVerifierInformationEx = 0x005C,
        SystemTimeZoneInformation = 0x005D,
        SystemImageFileExecutionOptionsInformation = 0x005E,
        SystemCoverageInformation = 0x005F,
        SystemPrefetchPatchInformation = 0x0060,
        SystemVerifierFaultsInformation = 0x0061,
        SystemSystemPartitionInformation = 0x0062,
        SystemSystemDiskInformation = 0x0063,
        SystemProcessorPerformanceDistribution = 0x0064,
        SystemNumaProximityNodeInformation = 0x0065,
        SystemDynamicTimeZoneInformation = 0x0066,
        SystemCodeIntegrityInformation = 0x0067,
        SystemProcessorMicrocodeUpdateInformation = 0x0068,
        SystemProcessorBrandString = 0x0069,
        SystemVirtualAddressInformation = 0x006A,
        SystemLogicalProcessorAndGroupInformation = 0x006B,
        SystemProcessorCycleTimeInformation = 0x006C,
        SystemStoreInformation = 0x006D,
        SystemRegistryAppendString = 0x006E,
        SystemAitSamplingValue = 0x006F,
        SystemVhdBootInformation = 0x0070,
        SystemCpuQuotaInformation = 0x0071,
        SystemNativeBasicInformation = 0x0072,
        SystemErrorPortTimeouts = 0x0073,
        SystemLowPriorityIoInformation = 0x0074,
        SystemBootEntropyInformation = 0x0075,
        SystemVerifierCountersInformation = 0x0076,
        SystemPagedPoolInformationEx = 0x0077,
        SystemSystemPtesInformationEx = 0x0078,
        SystemNodeDistanceInformation = 0x0079,
        SystemAcpiAuditInformation = 0x007A,
        SystemBasicPerformanceInformation = 0x007B,
        SystemQueryPerformanceCounterInformation = 0x007C,
        SystemSessionBigPoolInformation = 0x007D,
        SystemBootGraphicsInformation = 0x007E,
        SystemScrubPhysicalMemoryInformation = 0x007F,
        SystemBadPageInformation = 0x0080,
        SystemProcessorProfileControlArea = 0x0081,
        SystemCombinePhysicalMemoryInformation = 0x0082,
        SystemEntropyInterruptTimingInformation = 0x0083,
        SystemConsoleInformation = 0x0084,
        SystemPlatformBinaryInformation = 0x0085,
        SystemThrottleNotificationInformation = 0x0086,
        SystemHypervisorProcessorCountInformation = 0x0087,
        SystemDeviceDataInformation = 0x0088,
        SystemDeviceDataEnumerationInformation = 0x0089,
        SystemMemoryTopologyInformation = 0x008A,
        SystemMemoryChannelInformation = 0x008B,
        SystemBootLogoInformation = 0x008C,
        SystemProcessorPerformanceInformationEx = 0x008D,
        SystemSpare0 = 0x008E,
        SystemSecureBootPolicyInformation = 0x008F,
        SystemPageFileInformationEx = 0x0090,
        SystemSecureBootInformation = 0x0091,
        SystemEntropyInterruptTimingRawInformation = 0x0092,
        SystemPortableWorkspaceEfiLauncherInformation = 0x0093,
        SystemFullProcessInformation = 0x0094,
        MaxSystemInfoClass = 0x0095
    }

    public enum ProcessorArchitecture
    {
        X86 = 0,
        X64 = 9,
        @Arm = -1,
        Itanium = 6,
        Unknown = 0xFFFF,
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct SystemInfo
    {
        public ProcessorArchitecture ProcessorArchitecture; // WORD
        public uint PageSize; // DWORD
        public IntPtr MinimumApplicationAddress; // (long)void*
        public IntPtr MaximumApplicationAddress; // (long)void*
        public IntPtr ActiveProcessorMask; // DWORD*
        public uint NumberOfProcessors; // DWORD (WTF)
        public uint ProcessorType; // DWORD
        public uint AllocationGranularity; // DWORD
        public ushort ProcessorLevel; // WORD
        public ushort ProcessorRevision; // WORD
    }

    [StructLayout(LayoutKind.Sequential, Size = 40)]
    public struct PROCESS_MEMORY_COUNTERS_EX
    {
        public uint cb;             // The size of the structure, in bytes (DWORD).
        public uint PageFaultCount;         // The number of page faults (DWORD).
        public IntPtr PeakWorkingSetSize;     // The peak working set size, in bytes (SIZE_T).
        public IntPtr WorkingSetSize;         // The current working set size, in bytes (SIZE_T).
        public IntPtr QuotaPeakPagedPoolUsage;    // The peak paged pool usage, in bytes (SIZE_T).
        public IntPtr QuotaPagedPoolUsage;    // The current paged pool usage, in bytes (SIZE_T).
        public IntPtr QuotaPeakNonPagedPoolUsage; // The peak nonpaged pool usage, in bytes (SIZE_T).
        public IntPtr QuotaNonPagedPoolUsage;     // The current nonpaged pool usage, in bytes (SIZE_T).
        public IntPtr PagefileUsage;          // The Commit Charge value in bytes for this process (SIZE_T). Commit Charge is the total amount of memory that the memory manager has committed for a running process.
        public IntPtr PeakPagefileUsage;      // The peak value in bytes of the Commit Charge during the lifetime of this process (SIZE_T).
        public IntPtr PrivateUsage;
    }
#else
    //Dummy implementation if _APP_PERFORMANCE_ is not defined
    internal static class MEMData
    {
        public static Int64 GetMEMUsage()
        {
            return 0;
        }
    }

    internal static class CPUData
    {
        public static double GetCPUUsage()
        {
            return 0;
        }
    }

#endif
}
