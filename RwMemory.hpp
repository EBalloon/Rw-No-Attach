    uint64_t OldAttach; Use this for a long read
     
    uint64_t GetDirectoryTableBase(PEPROCESS Process)
    {
    	return *(uint64_t*)(uint64_t(Process) + 0x28);
    }
     
    void AttachProcess(PEPROCESS Process, uint64_t* OldAttach, bool IsAttached = false)
    {
    	uint64_t DirectoryTableBase;
    	uint64_t result;
    	uint64_t Value;
     
    	if (IsAttached)
    	{
    		//Attach to Process
    		*OldAttach = *(uint64_t*)(uint64_t(CurrentThread) + 0xB8);
    		*(uint64_t*)(uint64_t(CurrentThread) + 0xB8) = uint64_t(Process);
    	}
     
    	//Get DirectoryTableBase;
    	DirectoryTableBase = *(uint64_t*)(uint64_t(Process) + 0x28);
    	if ((DirectoryTableBase & 2) != 0)
    		DirectoryTableBase = DirectoryTableBase | 0x8000000000000000u;
     
    	// Write offset to DirectoryTableBase
    	__writegsqword(0x9000u, DirectoryTableBase);
    	__writecr3(DirectoryTableBase);
     
    	// Temp Control Register
    	Value = __readcr4();
    	if ((Value & 0x20080) != 0)
    	{
    		result = Value ^ 0x80;
    		__writecr4(Value ^ 0x80);
    		__writecr4(Value);
    	}
    	else
    	{
    		result = __readcr3();
    		__writecr3(result);
    	}
    }
     
    void DetachProcess(PEPROCESS Process, uint64_t OldAttach, bool IsAttached = false)
    {	
    	auto CurrentRunTime = *(ULONG*)(uint64_t(CurrentThread) + 0x50);
    	auto ExpectedRunTime = *(ULONG*)(uint64_t(CurrentThread) + 0x54);
     
    	if (IsAttached)
    	{
    		// restore to the old
    		*(uint64_t*)(uint64_t(CurrentThread) + 0xB8) = OldAttach;
    	}
     
    	auto Time = CurrentRunTime / ExpectedRunTime;
    	if (!Time || Time > 4) Time = 1;
     
    	// Due to the bad code we will put a sleep
    	Sleep(Time);
    }
     
    NTSTATUS ReadVirtualMemory(
    	PEPROCESS Process,
    	PVOID Destination,
    	PVOID Source,
    	SIZE_T Size)
    {
    	NTSTATUS ntStatus = STATUS_SUCCESS;
    	KAPC_STATE ApcState;
    	PHYSICAL_ADDRESS SourcePhysicalAddress;
    	PVOID MappedIoSpace;
    	PVOID MappedKva;
    	PMDL Mdl;
    	BOOLEAN ShouldUseSourceAsUserVa;
     
    	ShouldUseSourceAsUserVa = Source <= MmHighestUserAddress ? TRUE : FALSE;
     
    	// 1. Attach to the process
    	//    Sets specified process's PML4 to the CR3
    	uint64_t OldAttach;
    	AttachProcess(Process, &OldAttach, true);
     
    	// 2. Get the physical address corresponding to the user virtual memory
    	SourcePhysicalAddress = MmGetPhysicalAddress(
    		ShouldUseSourceAsUserVa == TRUE ? Source : Destination);
     
    	// 3. Detach from the process
    	//    Restores previous the current thread
    	DetachProcess(Process, OldAttach, true);
     
    	if (!SourcePhysicalAddress.QuadPart)
    	{
    		return STATUS_INVALID_ADDRESS;
    	}
     
    	// 4. Map an IO space for MDL
    	MappedIoSpace = MmMapIoSpace(SourcePhysicalAddress, Size, MmNonCached);
    	if (!MappedIoSpace)
    	{
    		return STATUS_INSUFFICIENT_RESOURCES;
    	}
     
    	// 5. Allocate MDL
    	Mdl = IoAllocateMdl(MappedIoSpace, (ULONG)Size, FALSE, FALSE, NULL);
    	if (!Mdl)
    	{
    		MmUnmapIoSpace(MappedIoSpace, Size);
    		return STATUS_INSUFFICIENT_RESOURCES;
    	}
     
    	// 6. Build MDL for non-paged pool
    	MmBuildMdlForNonPagedPool(Mdl);
     
    	// 7. Map to the KVA
    	MappedKva = MmMapLockedPagesSpecifyCache(
    		Mdl,
    		KernelMode,
    		MmNonCached,
    		NULL,
    		FALSE,
    		NormalPagePriority);
     
    	if (!MappedKva)
    	{
    		MmUnmapIoSpace(MappedIoSpace, Size);
    		IoFreeMdl(Mdl);
    		return STATUS_INSUFFICIENT_RESOURCES;
    	}
     
    	// 8. copy memory
    	memcpy(
    		ShouldUseSourceAsUserVa == TRUE ? Destination : MappedKva,
    		ShouldUseSourceAsUserVa == TRUE ? MappedKva : Destination,
    		Size);
     
    	MmUnmapIoSpace(MappedIoSpace, Size);
    	MmUnmapLockedPages(MappedKva, Mdl);
    	IoFreeMdl(Mdl);
     
    	return STATUS_SUCCESS;
    }
     
    NTSTATUS ReadProcessMemory(HANDLE ProcessPid, PVOID Address, PVOID Buffer, SIZE_T Size)
    {
    	PEPROCESS Process = { 0 };
    	auto ntStatus = PsLookupProcessByProcessId(ProcessPid, &Process);
    	if (NT_SUCCESS(ntStatus) && Process)
    	{
    		ntStatus = ReadVirtualMemory(Process, Buffer, Address, Size);
    	}
     
    	ObDereferenceObject(Process);
    	return ntStatus;
    }
