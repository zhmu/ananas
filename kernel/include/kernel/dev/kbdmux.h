#ifndef __ANANAS_DEV_KBDMUX_H__
#define __ANANAS_DEV_KBDMUX_H__

#include <ananas/util/list.h>

namespace keyboard_mux {

namespace modifier {

static inline constexpr int Shift = 1;
static inline constexpr int Control = 2;
static inline constexpr int Alt = 4;

}

namespace code {

static inline constexpr int F1  = 300;
static inline constexpr int F2  = 301;
static inline constexpr int F3  = 302;
static inline constexpr int F4  = 303;
static inline constexpr int F5  = 304;
static inline constexpr int F6  = 305;
static inline constexpr int F7  = 306;
static inline constexpr int F8  = 307;
static inline constexpr int F9  = 308;
static inline constexpr int F10 = 309;
static inline constexpr int F11 = 310;
static inline constexpr int F12 = 311;

}

struct Key
{
	enum class Type {
		Invalid,
		Character,
		Special,
	};

	constexpr Key() = default;
	constexpr Key(Type t, int c) : type(t), ch(c)
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
	virtual void OnKey(const Key& key, int modifiers) = 0;
};

void RegisterConsumer(IKeyboardConsumer& consumer);
void UnregisterConsumer(IKeyboardConsumer& consumer);
void OnKey(const Key& key, int modifiers);

}

#endif /* __ANANAS_DEV_KBDMUX_H__ */
