#pragma once
#include "sched/process.hpp"
#include "optional.hpp"

void service_create(kstd::vector<kstd::string>&& features, Process* process);
void service_remove(Process* process);
kstd::shared_ptr<ProcessDescriptor> service_get(const kstd::vector<kstd::string>& needed_features);
