#pragma once

#include <memory>
#include <vector>

namespace chimera::fx
{
class Processor
{
public:
    virtual ~Processor() = default;
    virtual float process(float input) = 0;
};

class FxChain
{
public:
    void add(std::unique_ptr<Processor> processor);
    float process(float input);
    int size() const;

private:
    std::vector<std::unique_ptr<Processor>> processors;
};
}
