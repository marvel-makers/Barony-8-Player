#pragma once

#include "transport.hpp"

#include <memory>

namespace barony::net
{
std::unique_ptr<ITransport> makeEosTransport();
}
