
### Example:

    Status = ReadProcessMemory(ProcessPid, Address, &Buffer, Size);
    if (NT_SUCCESS(Status) && Buffer)
        Printf("Value: %llu\n", Buffer);
