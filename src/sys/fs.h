#pragma once
#include "dev.h"
#include "crescent/fs.h"

typedef struct Vfs Vfs;

typedef struct {
	GenericDevice generic;
	Vfs* vfs;
} PartitionDev;

int partition_dev_devmsg(PartitionDev* self, DevMsgFs msg, __user void* data);
