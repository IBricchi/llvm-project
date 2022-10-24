extern "C" void my_inline_info(void* CB, void* MandatoryOnly, void* prediction)
{
    __builtin_printf("Hello\n");
}