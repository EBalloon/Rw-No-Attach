# RWM-No-Attach




  template < typename Type >
  Type Read(Type address, uint32_t offset) noexcept {

	  if (address == Type(0)) {
	  	return Type(0);
  	}

  	return *reinterpret_cast<Type*>(uintptr_t(address) + offset);
  }

  typedef struct _EThread
  {
  	typedef struct _KAPC_STATE
	  {
	  	struct _LIST_ENTRY ApcListHead[2];
	  } KAPC_STATE, * PKAPC_STATE, * PRKAPC_STATE;

	  char pad_0x00[0x98]; // KTHREAD->ApcState // Windows 10 | 2004 - 20H1
	  KAPC_STATE ApcState;

  } EThread, * PEThread;
  PEThread CurrentThread;

  uint64_t GetDirectoryTableBase(PEPROCESS Process)
  {
	  return Read<uint64_t>(uint64_t(Process), 0x28);
  }

  void AttachProcess(PEPROCESS Process, PRKAPC_STATE ApcState)
  {
	  InitializeListHead(&CurrentThread->ApcState.ApcListHead[KernelMode]);
	  InitializeListHead(&CurrentThread->ApcState.ApcListHead[UserMode]);

  	auto DirectoryTableBase = GetDirectoryTableBase(Process);
	  if ((DirectoryTableBase & 2) != 0)
	  	DirectoryTableBase = DirectoryTableBase | 0x8000000000000000ui64;

  	__writegsqword(0x9000u, DirectoryTableBase);
	  __writecr3(DirectoryTableBase);

	  auto Value = __readcr4();
	  if ((Value & 0x20080) != 0)
	  {
	  	__writecr4(Value ^ 0x80);
		  __writecr4(Value);
	  }
	  else
  	{
	  	Value = __readcr3();
	  	__writecr3(Value);
  	}
  }

  void detachProcess(PRKAPC_STATE ApcState)
  {
	  Sleep(1);
	  RemoveHeadList(&CurrentThread->ApcState.ApcListHead[KernelMode]);
	  RemoveHeadList(&CurrentThread->ApcState.ApcListHead[UserMode]);
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

	  if (NT_SUCCESS(ntStatus) && Process)
	  {
		  ShouldUseSourceAsUserVa = Source <= MmHighestUserAddress ? TRUE : FALSE;

	  	// 2. Get the physical address corresponding to the user virtual memory
		  SourcePhysicalAddress = MmGetPhysicalAddress(
			  ShouldUseSourceAsUserVa == TRUE ? Source : Destination);
  
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
	  }

	  return ntStatus;
  }


  NTSTATUS ReadProcessMemory(PEPROCESS Process, PVOID Address, PVOID Buffer, SIZE_T Size)
  {
  	KAPC_STATE ApcState;

  	CurrentThread = PEThread(KeGetCurrentThread());

  	AttachProcess(Process, &ApcState);
  	auto Status = ReadVirtualMemory(Process, Buffer, Address, Size);
	  detachProcess(&ApcState);

	  return Status;
  }


  Example:

  Status = ReadProcessMemory(Process, TargetAddress, &SourceAddress, Size);
  Printf("Value: %llu\n", SourceAddress);
