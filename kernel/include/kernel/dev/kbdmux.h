#ifndef __ANANAS_DEV_KBDMUX_H__
#define __ANANAS_DEV_KBDMUX_H__

#include <ananas/util/list.h>

namespace keyboard_mux {

struct Key
{
	enum class Type {
		Invalid,
		Character,
		Control
	};

	constexpr Key() = default;
	Key(Type t, int c) : type(t), ch(c)
	{
	}

	bool IsValid() const
	{
		return type != Type::Invalid;
	}

	Type type = Type::Invalid;
	int ch = 0;
};

class IKeyboardConsumer : public util::List<IKeyboardConsumer>::NodePtr
{
public:
	virtual void OnKey(const Key& key) = 0;
};

void RegisterConsumer(IKeyboardConsumer& consumer);
void UnregisterConsumer(IKeyboardConsumer& consumer);
void OnKey(const Key& key);

}

#endif /* __ANANAS_DEV_KBDMUX_H__ */
