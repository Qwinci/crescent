#include "cmdline.h"
#include "stdio.h"

static Str get_arg(Str* cmdline) {
	Str str = {.data = cmdline->data};
	for (; cmdline->len && *cmdline->data != ' '; --cmdline->len, ++cmdline->data);
	str.len = cmdline->data - str.data;
	return str;
}

KernelCmdline parse_kernel_cmdline(const char* cmdline) {
	KernelCmdline res = {};
	Str str = str_new(cmdline);
	while (str.len) {
		if (str_strip_prefix(&str, str_new("init="))) {
			Str value = get_arg(&str);
			res.init = value;
		}
		else if (str_strip_prefix(&str, str_new("native_init="))) {
			Str value = get_arg(&str);
			res.init = value;
			res.native_init = true;
		}
		else if (str_strip_prefix(&str, str_new("posix_root="))) {
			Str value = get_arg(&str);
			res.posix_root = value;
		}
		else {
			str.len -= 1;
			str.data += 1;
		}
	}

	return res;
}
