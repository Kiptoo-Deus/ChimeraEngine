#include "fx/FxChain.h"

namespace chimera::fx
{
void FxChain::add(std::unique_ptr<Processor> processor)
{
    processors.push_back(std::move(processor));
}

float FxChain::process(float input)
{
    auto value = input;
    for (auto& processor : processors)
        value = processor->process(value);
    return value;
}

int FxChain::size() const
{
    return static_cast<int>(processors.size());
}
}
