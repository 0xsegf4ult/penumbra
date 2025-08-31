export module arch.x86_64:cpu;
import types;

export class CPU
{
public:
	[[noreturn]] static void halt()
	{
		for(;;)
		{
			asm volatile("cli; hlt");
		}
	}
};
