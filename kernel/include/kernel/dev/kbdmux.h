#ifndef __ANANAS_DEV_KBDMUX_H__
#define __ANANAS_DEV_KBDMUX_H__

#include <ananas/util/list.h>

namespace keyboard_mux {

class IKeyboardConsumer : public util::List<IKeyboardConsumer>::NodePtr
{
public:
	virtual void OnCharacter(int ch) = 0;
};

void RegisterConsumer(IKeyboardConsumer& consumer);
void UnregisterConsumer(IKeyboardConsumer& consumer);
void OnCharacter(int ch);

}

#endif /* __ANANAS_DEV_KBDMUX_H__ */
