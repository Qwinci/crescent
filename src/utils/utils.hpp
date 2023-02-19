#pragma once


#define container_of(ptr, base, field) (cast<base*>(cast<u8*>(ptr) - offsetof(base, field)))