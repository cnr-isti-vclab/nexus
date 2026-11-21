#include "prioritizer.h"
#include "token.h"
#include "controller.h"

using namespace nx;

//traverse the dag computing errors and assigning priorities
void Prioritizer::prioritize(Nexus *nx, uint32_t frame) {
    current_frame = frame;
    prefetched = 0;
    if(!nx->isReady()) return;
    assert(nx->controller);
    traverse(nx);
}

Traversal::Action Prioritizer::expand(HeapNode h) {

    if(prefetched > prefetch) return STOP; //stop traversal

    if(h.error < target_error)
        prefetched++;

    Token &token = nexus->tokens[h.node];
    Priority newprio = Priority(h.error, current_frame);
    Priority oldprio = token.getPriority();
    if(oldprio < newprio)
        token.setPriority(newprio);

    //token will be added only if not already present and thread safe.
    assert(nexus->controller);
    nexus->controller->addToken(&token);

    return EXPAND;
}
