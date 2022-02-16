
OK, let's go.
I did it in less than a day because I was bored.
The code isn't perfect, but it does the job!

you can use other apis like you use when using KeAttachProcess/KeStackAttachProcess 


### Example:

    Status = ReadProcessMemory(ProcessPid, Address, &Buffer, Size);
    if (NT_SUCCESS(Status) && Buffer)
        Printf("Value: %llu\n", Buffer);

### Tested on Windows 10 2004 - 20H1
