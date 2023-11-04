#include "storage.h"
#include "gpt.h"

Storage* STORAGES = NULL;

void storage_register(Storage* storage) {
	storage->next = STORAGES;
	STORAGES = storage;
}

void storage_enum_partitions(Storage* self) {
	gpt_enum_partitions(self);
}