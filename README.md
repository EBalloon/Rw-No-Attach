
### Example:

     Status = ReadProcessMemory(ProcessPid, Address, &Address, Size);
     if (NT_SUCCESS(Status) && Address)
         Printf("Value: %llu\n", Address);
