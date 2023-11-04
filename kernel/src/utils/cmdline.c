#include "cmdline.h"
#include "stdio.h"

static Str get_arg(Str* cmdline) {
	Str str = {.data = cmdline->data};
	for (; cmdline->len && *cmdline->data != ' '; --cmdline->len, ++cmdline->data);
	str.len = cmdline->data - str.data;
	if (cmdline->len) {
		str.len -= 1;
	}
	return str;
}

KernelCmdline parse_kernel_cmdline(const char* cmdline) {
	KernelCmdline res = {};
	Str str = str_new(cmdline);
	if (str_strip_prefix(&str, str_new("init="))) {
		Str value = get_arg(&str);
		res.init = value;
	}

	return res;
}
