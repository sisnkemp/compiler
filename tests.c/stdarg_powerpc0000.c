typedef __builtin_va_list va_list;

int
main(void)
{
	va_list ap;

	ap->gpr = 0;
	ap->fpr = 0;
	ap->overflow_arg_area = 0;
	ap->reg_save_area = 0;
	return 0;
}
