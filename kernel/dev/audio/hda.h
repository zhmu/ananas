#ifndef __ANANAS_HDA_H__
#define __ANANAS_HDA_H__

#include <ananas/types.h>
#include <ananas/util/list.h>
#include "kernel/device.h"

class Result;

namespace hda {

struct PlayContext;

#define HDA_DEBUG(fmt, ...) \
	device_printf(dev, fmt, __VA_ARGS__)

#define HDA_MAKE_PAYLOAD_ID4(id4, data16) \
	(((id4) << 16) | (data16))
#define HDA_MAKE_PAYLOAD_ID12(id12, data8) \
	(((id12) << 8) | (data8))

#define HDA_MAKE_VERB(cad, nodeid, payload) \
	(((cad) << 28) | ((nodeid) << 20) | (payload))

#define HDA_MAKE_VERB_NODE(n, payload) \
	HDA_MAKE_VERB((n).n_address.na_cad, (n).n_address.na_nid, (payload))

#define HDA_CODEC_CMD_GETPARAM	0xF00
#define  HDA_CODEC_PARAM_VENDORID	0x00
#define   HDA_PARAM_VENDORID_VENDORID(x)	((x) >> 16)
#define   HDA_PARAM_VENDORID_DEVICEID(x)	((x) & 0xffff)
#define  HDA_CODEC_PARAM_REVISIONID	0x02
#define   HDA_PARAM_REVISIONID_MAJREV(x)	(((x) >> 20) & 15)
#define   HDA_PARAM_REVISIONID_MINREV(x)	(((x) >> 16) & 15)
#define   HDA_PARAM_REVISIONID_REVISIONID(x)	(((x) >> 8) & 255)
#define   HDA_PARAM_REVISIONID_STEPPINGID(x)	((x) & 255)
#define  HDA_CODEC_PARAM_SUBNODECOUNT	0x04
#define   HDA_PARAM_SUBNODECOUNT_START(x)	(((x) >> 16) & 255)
#define   HDA_PARAM_SUBNODECOUNT_TOTAL(x)	((x) & 255)
#define  HDA_CODEC_PARAM_FGT		0x05
#define   HDA_PARAM_FGT_UNSOL_CAPABLE(x)	(((x) >> 8) & 1)
#define   HDA_PARAM_FGT_NODETYPE(x)		((x) & 255)
#define   HDA_PARAM_FGT_NODETYPE_AUDIO		1
#define   HDA_PARAM_FGT_NODETYPE_MODEM		2
#define  HDA_CODEC_PARAM_AGT_CAPS	0x08
#define   HDA_PARAM_AGT_CAPS_BEEPGEN(x)		(((x) >> 16) & 1)
#define   HDA_PARAM_AGT_CAPS_INPUTDELAY(x)	(((x) >> 8) & 15)
#define   HDA_PARAM_AGT_CAPS_OUTPUTDELAY(x)	((x) & 15)
#define  HDA_CODEC_PARAM_AW_CAPS	0x09
#define   HDA_PARAM_AW_CAPS_TYPE(x)		(((x) >> 20) & 15)
#define    HDA_PARAM_AW_CAPS_TYPE_AUDIO_OUT		0
#define    HDA_PARAM_AW_CAPS_TYPE_AUDIO_IN 		1
#define    HDA_PARAM_AW_CAPS_TYPE_AUDIO_MIXER 		2
#define    HDA_PARAM_AW_CAPS_TYPE_AUDIO_SELECTOR 	3
#define    HDA_PARAM_AW_CAPS_TYPE_AUDIO_PINCOMPLEX 	4
#define    HDA_PARAM_AW_CAPS_TYPE_AUDIO_POWER	 	5
#define    HDA_PARAM_AW_CAPS_TYPE_AUDIO_VOLUMEKNOB	6
#define    HDA_PARAM_AW_CAPS_TYPE_AUDIO_BEEPGENERATOR	7
#define    HDA_PARAM_AW_CAPS_TYPE_AUDIO_VENDORDEFINED 15
#define   HDA_PARAM_AW_CAPS_DELAY(x)		(((x) >> 16) & 15)
#define   HDA_PARAM_AW_CAPS_CHANCOUNTEXT(x)	(((x) >> 13) & 7)
#define   HDA_PARAM_AW_CAPS_CPCAPS(x)		(((x) >> 12) & 1)
#define   HDA_PARAM_AW_CAPS_LRSWAP(x)		(((x) >> 11) & 1)
#define   HDA_PARAM_AW_CAPS_POWERCNTRL(x)	(((x) >> 10) & 1)
#define   HDA_PARAM_AW_CAPS_DIGITAL(x)		(((x) >> 9) & 1)
#define   HDA_PARAM_AW_CAPS_CONNLIST(x)		(((x) >> 8) & 1)
#define   HDA_PARAM_AW_CAPS_UNSOLCAPABLE(x)	(((x) >> 7) & 1)
#define   HDA_PARAM_AW_CAPS_PROCWIDGET(x)	(((x) >> 6) & 1)
#define   HDA_PARAM_AW_CAPS_STRIPE(x)		(((x) >> 5) & 1)
#define   HDA_PARAM_AW_CAPS_FORMATOVERRIDE(x)	(((x) >> 4) & 1)
#define   HDA_PARAM_AW_CAPS_AMPPARAMOVERRIDE(x)	(((x) >> 3) & 1)
#define   HDA_PARAM_AW_CAPS_OUTAMPPRESENT(x)	(((x) >> 2) & 1)
#define   HDA_PARAM_AW_CAPS_INAMPPRESENT(x)	(((x) >> 1) & 1)
#define   HDA_PARAM_AW_CAPS_CHANCOUNTLSB(x)	((x) & 1)
#define  HDA_CODEC_PARAM_PCM 0x0a
#define   HDA_CODEC_PARAM_PCM_B32(x)		(((x) >> 20) & 1)
#define   HDA_CODEC_PARAM_PCM_B24(x)		(((x) >> 19) & 1)
#define   HDA_CODEC_PARAM_PCM_B20(x)		(((x) >> 18) & 1)
#define   HDA_CODEC_PARAM_PCM_B16(x)		(((x) >> 17) & 1)
#define   HDA_CODEC_PARAM_PCM_B8(x)		(((x) >> 16) & 1)
#define   HDA_CODEC_PARAM_PCM_R_8_0(x)		((x) & 1)
#define   HDA_CODEC_PARAM_PCM_R_11_025(x)	(((x) >> 1) & 1)
#define   HDA_CODEC_PARAM_PCM_R_16_0(x)		(((x) >> 2) & 1)
#define   HDA_CODEC_PARAM_PCM_R_22_05(x)	(((x) >> 3) & 1)
#define   HDA_CODEC_PARAM_PCM_R_32_0(x)		(((x) >> 4) & 1)
#define   HDA_CODEC_PARAM_PCM_R_44_1(x)		(((x) >> 5) & 1)
#define   HDA_CODEC_PARAM_PCM_R_48_0(x)		(((x) >> 6) & 1)
#define   HDA_CODEC_PARAM_PCM_R_88_2(x)		(((x) >> 7) & 1)
#define   HDA_CODEC_PARAM_PCM_R_96_0(x)		(((x) >> 8) & 1)
#define   HDA_CODEC_PARAM_PCM_R_176_4(x)	(((x) >> 9) & 1)
#define   HDA_CODEC_PARAM_PCM_R_192_0(x)	(((x) >> 10) & 1)
#define   HDA_CODEC_PARAM_PCM_R_384_0(x)	(((x) >> 11) & 1)
#define  HDA_CODEC_PARAM_PINCAP 0x0c
#define   HDA_CODEC_PINCAP_HBR(x)		(((x) >> 27) & 1)
#define   HDA_CODEC_PINCAP_DP(x)		(((x) >> 24) & 1)
#define   HDA_CODEC_PINCAP_EAPD(x)		(((x) >> 16) & 1)
#define   HDA_CODEC_PINCAP_VREF_CONTROL(x)	(((x) >> 8) & 255)
#define   HDA_CODEC_PINCAP_HDMI(x)		(((x) >> 7) & 1)
#define   HDA_CODEC_PINCAP_BALANCED_IO(x)	(((x) >> 6) & 1)
#define   HDA_CODEC_PINCAP_INPUT(x)		(((x) >> 5) & 1)
#define   HDA_CODEC_PINCAP_OUTPUT(x)		(((x) >> 4) & 1)
#define   HDA_CODEC_PINCAP_HEADPHONE(x)		(((x) >> 3) & 1)
#define   HDA_CODEC_PINCAP_PRESENCE(x)		(((x) >> 2) & 1)
#define   HDA_CODEC_PINCAP_TRIGGER(x)		(((x) >> 1) & 1)
#define   HDA_CODEC_PINCAP_IMPEDANCE(x)		((x) & 1)
#define  HDA_CODEC_PARAM_AMPCAP_IN 0x0d
#define  HDA_CODEC_PARAM_AMPCAP_OUT 0x12
#define   HDA_CODEC_AMPCAP_MUTE(x)		(((x) >> 31) & 1)
#define   HDA_CODEC_AMPCAP_STEPSIZE(x)		(((x) >> 16) & 127)
#define   HDA_CODEC_AMPCAP_NUMSTEPS(x)		(((x) >> 8) & 127)
#define   HDA_CODEC_AMPCAP_OFFSET(x)		((x) & 127)
#define  HDA_CODEC_PARAM_CONLISTLEN 0xe
#define   HDA_CODEC_PARAM_CONLISTLEN_LONGFORM(x)	(((x) >> 7) & 1)
#define   HDA_CODEC_PARAM_CONLISTLEN_LENGTH(x)		((x) & 127)
#define  HDA_CODEC_PARAM_POWERSTATES 0xf
#define   HDA_CODEC_PARAM_POWERSTATES_EPSS(x)		(((x) >> 31) & 1)
#define   HDA_CODEC_PARAM_POWERSTATES_CLKSTOP(x)	(((x) >> 30) & 1)
#define   HDA_CODEC_PARAM_POWERSTATES_S3D3COLDSUP(x)	(((x) >> 29) & 1)
#define   HDA_CODEC_PARAM_POWERSTATES_D3COLDSUP(x)	(((x) >> 4) & 1)
#define   HDA_CODEC_PARAM_POWERSTATES_D3CSUP(x)		(((x) >> 3) & 1)
#define   HDA_CODEC_PARAM_POWERSTATES_D2CSUP(x)		(((x) >> 2) & 1)
#define   HDA_CODEC_PARAM_POWERSTATES_D1CSUP(x)		(((x) >> 1) & 1)
#define   HDA_CODEC_PARAM_POWERSTATES_D0CSUP(x)		(((x) >> 0) & 1)

#define HDA_CODEC_CMD_GETCONNSELECT	0xf01
#define HDA_CODEC_CMD_SETCONNSELECT	0x701
#define HDA_CODEC_CMD_GETCONNLISTENTRY	0xf02

#define HDA_CODEC_CMD_GETCONVCONTROL	0xf06
#define HDA_CODEC_CMD_SETCONVCONTROL	0x706
#define  HDA_CODEC_CONVCONTROL_STREAM(x)	((x) << 4)
#define  HDA_CODEC_CONVCONTROL_CHANNEL(x)	(x)
#define HDA_CODEC_CMD_GETPINSENSE	0xf09
#define HDA_CODEC_CMD_SETPINCONTROL	0x707
# define  HDA_CODEC_PINCONTROL_HENABLE		(1 << 7)
# define  HDA_CODEC_PINCONTROL_OUTENABLE	(1 << 6)
# define  HDA_CODEC_PINCONTROL_INENABLE		(1 << 5)
# define  HDA_CODEC_PINCONTROL_VREFEN(x)	(x)
#define HDA_CODEC_CMD_GETPINCONTROL	0x707

#define HDA_CODEC_CMD_SET_CONV_FORMAT	0x2
#define HDA_CODEC_CMD_GET_CONV_FORMAT	0xa

#define HDA_CODEC_CMD_SET_AMP_GAINMUTE	0x3
#define HDA_CODEC_CMD_GET_AMP_GAINMUTE	0xb
#define  HDA_CODEC_AMP_GAINMUTE_OUTPUT	(1 << 15)
#define  HDA_CODEC_AMP_GAINMUTE_INPUT	(1 << 14)
#define  HDA_CODEC_AMP_GAINMUTE_LEFT	(1 << 13)
#define  HDA_CODEC_AMP_GAINMUTE_RIGHT	(1 << 12)
#define  HDA_CODEC_AMP_GAINMUTE_INDEX(x)	((x) << 8)
#define  HDA_CODEC_AMP_GAINMUTE_MUTE	(1 << 7)
#define  HDA_CODEC_AMP_GAINMUTE_GAIN(x)	(x)

#define HDA_CODEC_CMD_GETCFGDEFAULT	0xf1c
#define  HDA_CODEC_CFGDEFAULT_PORTCONNECTIVITY(x)	(((x) >> 30) & 3)
#define   HDA_CODEC_CFGDEFAULT_PORTCONNECTIVITY_JACK	0
#define   HDA_CODEC_CFGDEFAULT_PORTCONNECTIVITY_NONE	1
#define   HDA_CODEC_CFGDEFAULT_PORTCONNECTIVITY_FIXED	2
#define   HDA_CODEC_CFGDEFAULT_PORTCONNECTIVITY_BOTH	3
#define  HDA_CODEC_CFGDEFAULT_LOCATION(x)		(((x) >> 24) & 63)
#define  HDA_CODEC_CFGDEFAULT_DEFAULTDEVICE(x)		(((x) >> 20) & 15)
#define   HDA_CODEC_CFGDEFAULT_DEFAULTDEVICE_LINE_OUT	0
#define   HDA_CODEC_CFGDEFAULT_DEFAULTDEVICE_SPEAKER	1
#define   HDA_CODEC_CFGDEFAULT_DEFAULTDEVICE_HP_OUT	2
#define   HDA_CODEC_CFGDEFAULT_DEFAULTDEVICE_CD		3
#define   HDA_CODEC_CFGDEFAULT_DEFAULTDEVICE_SPDIF_OUT	4
#define   HDA_CODEC_CFGDEFAULT_DEFAULTDEVICE_DIG_OTHER_OUT	5
#define   HDA_CODEC_CFGDEFAULT_DEFAULTDEVICE_MODEM_LINE_SIDE	6
#define   HDA_CODEC_CFGDEFAULT_DEFAULTDEVICE_MODEM_HANDSET_SIDE	7
#define   HDA_CODEC_CFGDEFAULT_DEFAULTDEVICE_LINE_IN	8
#define   HDA_CODEC_CFGDEFAULT_DEFAULTDEVICE_AUX	9
#define   HDA_CODEC_CFGDEFAULT_DEFAULTDEVICE_MIC_IN	10
#define   HDA_CODEC_CFGDEFAULT_DEFAULTDEVICE_TELEPHONY	11
#define   HDA_CODEC_CFGDEFAULT_DEFAULTDEVICE_SPDIF_IN	12
#define   HDA_CODEC_CFGDEFAULT_DEFAULTDEVICE_DIGITAL_OTHER_IN	13
#define   HDA_CODEC_CFGDEFAULT_DEFAULTDEVICE_OTHER	15
#define  HDA_CODEC_CFGDEFAULT_CONNECTION_TYPE(x)	(((x) >> 16) & 15)
#define   HDA_CODEC_CFGDEFAULT_CONNECTION_TYPE_UNKNOWN	0
#define   HDA_CODEC_CFGDEFAULT_CONNECTION_TYPE_1_8_STEREO_MONO	1
#define   HDA_CODEC_CFGDEFAULT_CONNECTION_TYPE_1_4_STEREO_MONO	2
#define   HDA_CODEC_CFGDEFAULT_CONNECTION_TYPE_ATAPI	3
#define   HDA_CODEC_CFGDEFAULT_CONNECTION_TYPE_RCA	4
#define   HDA_CODEC_CFGDEFAULT_CONNECTION_TYPE_OPTICAL	5
#define   HDA_CODEC_CFGDEFAULT_CONNECTION_TYPE_OTHER_DIGITAL	6
#define   HDA_CODEC_CFGDEFAULT_CONNECTION_TYPE_OTHER_ANALOG	7
#define   HDA_CODEC_CFGDEFAULT_CONNECTION_TYPE_MULTICHANNEL_ANALOG	8
#define   HDA_CODEC_CFGDEFAULT_CONNECTION_TYPE_XLR	9
#define   HDA_CODEC_CFGDEFAULT_CONNECTION_TYPE_RJ11	10
#define   HDA_CODEC_CFGDEFAULT_CONNECTION_TYPE_COMBINATION	11
#define   HDA_CODEC_CFGDEFAULT_CONNECTION_TYPE_OTHER	15
#define  HDA_CODEC_CFGDEFAULT_COLOR(x)			(((x) >> 12) & 15)
#define   HDA_CODEC_CFGDEFAULT_COLOR_UNKNOWN		0
#define   HDA_CODEC_CFGDEFAULT_COLOR_BLACK		1
#define   HDA_CODEC_CFGDEFAULT_COLOR_GREY		2
#define   HDA_CODEC_CFGDEFAULT_COLOR_BLUE		3
#define   HDA_CODEC_CFGDEFAULT_COLOR_GREEN		4
#define   HDA_CODEC_CFGDEFAULT_COLOR_RED			5
#define   HDA_CODEC_CFGDEFAULT_COLOR_ORANGE		6
#define   HDA_CODEC_CFGDEFAULT_COLOR_YELLOW		7
#define   HDA_CODEC_CFGDEFAULT_COLOR_PURPLE		8
#define   HDA_CODEC_CFGDEFAULT_COLOR_PINK		9
#define   HDA_CODEC_CFGDEFAULT_COLOR_WHITE		14
#define   HDA_CODEC_CFGDEFAULT_COLOR_OTHER		15
#define  HDA_CODEC_CFGDEFAULT_MISC(x)			(((x) >> 8) & 15)
#define   HDA_CODEC_CFGDEFAULT_MISC_NO_PRESENCE_DETECT	(1 << 0)
#define  HDA_CODEC_CFGDEFAULT_DEFAULT_ASSOCIATION(x)	(((x) >> 4) & 15)
#define  HDA_CODEC_CFGDEFAULT_SEQUENCE(x)		((x) & 15)

#define HDA_RESP_EX_CODEC_ADDR(x)		((x) & 15)
#define HDA_RESP_EX_CODEC_UNSOLICITED(x)	(((x) >> 4) & 1)

#define STREAM_TYPE_PCM			(0 << 15)
#define STREAM_TYPE_NON_PCM		(1 << 15)
#define STREAM_BASE_48_0		(0 << 14)
#define STREAM_BASE_44_1		(1 << 14)
#define STREAM_MULT_x1			(0 << 11)
#define STREAM_MULT_x2			(1 << 11)
#define STREAM_MULT_x3			(2 << 11)
#define STREAM_MULT_x4			(3 << 11)
#define STREAM_DIV_1			(0 << 8)
#define STREAM_DIV_2			(1 << 8)
#define STREAM_DIV_3			(2 << 8)
#define STREAM_DIV_4			(3 << 8)
#define STREAM_DIV_5			(4 << 8)
#define STREAM_DIV_6			(5 << 8)
#define STREAM_DIV_7			(6 << 8)
#define STREAM_DIV_8			(7 << 8)
#define STREAM_BITS_8			(0 << 4)
#define STREAM_BITS_16			(1 << 4)
#define STREAM_BITS_20			(2 << 4)
#define STREAM_BITS_24			(3 << 4)
#define STREAM_BITS_32			(4 << 4)
#define STREAM_CHANNELS(x)		(x)

/* Retrieve the state change status register and clears it */

#define HDF_DIR_IN 0
#define HDF_DIR_OUT 1

class IHDAFunctions
{
public:
	typedef void* Context;
	virtual Result IssueVerb(uint32_t verb, uint32_t* resp, uint32_t* resp_ex) = 0;
	virtual uint16_t GetAndClearStateChange() = 0;
	virtual Result OpenStream(int tag, int dir, uint16_t fmt, int num_pages, Context* context) = 0;
	virtual Result CloseStream(Context context) = 0;
	virtual Result StartStreams(int num, Context* context) = 0;
	virtual Result StopStreams(int num, Context* context) = 0;
	virtual void* GetStreamDataPointer(Context context, int page) = 0;
};

enum HDANodeType {
	NT_Pin,
	NT_AudioOut,
	NT_AudioIn,
	NT_AudioMixer,
	NT_AudioSelector,
	NT_VendorDefined
};

class Node_AW;
class Output;

/*
 * Audio function group; contains pointers to all codec nodes therein.
 */
class AFG
{
public:
	int afg_cad;
	int afg_nid;
	uint32_t afg_pcm;
	util::List<Node_AW> afg_nodes;
	util::List<Output> afg_outputs;
};

class NodeAddress
{
public:
	NodeAddress(int cad, int nid)
	 : na_cad(cad), na_nid(nid)
	{
	}

	int na_cad;			/* Codec address of this node */
	int na_nid;			/* Node ID */
};

// Base type of all HDA nodes
class Node
{
public:
	Node(const NodeAddress& na)
	 : n_address(na)
	{
	}

	virtual HDANodeType GetType() const = 0;
	NodeAddress n_address;
};


/*
 * Plain audio widget with an optional connection list; these are always
 * connected to an audio function group.
 */
class Node_AW : public Node, public util::List<Node_AW>::NodePtr
{
public:
	Node_AW(const NodeAddress& na, uint32_t caps)
	 : Node(na), aw_caps(caps)
	{
	}

	uint32_t aw_caps;		/* Audio widget capabilities */
	int aw_num_conn = 0;		/* Number of connections */
	Node_AW** aw_conn = nullptr;	/* Connection list */
};

class Node_Pin : public Node_AW
{
public:
	using Node_AW::Node_AW;
	HDANodeType GetType() const override
	{
		return NT_Pin;
	}

	uint32_t p_cap;			/* Capabilities */
	uint32_t p_cfg;			/* Pin configuration */
};

class Node_AudioOut : public Node_AW
{
public:
	using Node_AW::Node_AW;
	HDANodeType GetType() const override
	{
		return NT_AudioOut;
	}

	uint32_t ao_pcm;		/* Output PCM capabilities */
};

class Node_AudioIn : public Node_AW
{
public:
	using Node_AW::Node_AW;
	HDANodeType GetType() const override
	{
		return NT_AudioIn;
	}
};

class Node_AudioMixer : public Node_AW
{
public:
	using Node_AW::Node_AW;
	HDANodeType GetType() const override
	{
		return NT_AudioMixer;
	}

	uint32_t am_ampcap_in;		/* Input amplifier capabilities */
	uint32_t am_ampcap_out;		/* Output amplifier capabilities */
};

class Node_AudioSelector : public Node_AW
{
public:
	using Node_AW::Node_AW;
	HDANodeType GetType() const override
	{
		return NT_AudioSelector;
	}
};

class Node_VendorDefined : public Node_AW
{
public:
	using Node_AW::Node_AW;
	HDANodeType GetType() const override
	{
		return NT_VendorDefined;
	}
};

/*
 * Used to group one or more pins together; this is mainly used for multi-pin
 * output groups.
 */
class PinGroup
{
public:
	PinGroup() = default;
	virtual ~PinGroup();

	int pg_count = 0;
	Node_Pin** pg_pin = nullptr;
};

/*
 * Output definition; contains output pins to use and the configuration in use.
 */
class Output : public util::List<Output>::NodePtr
{
public:
	int o_channels;
	uint32_t o_pcm_output; /* Output PCM capabilities */
	PinGroup* o_pingroup;
};

/* Output routing */
class RoutingPlan
{
public:
	RoutingPlan() = default;
	~RoutingPlan();

	int rp_num_nodes = 0;
	Node_AW** rp_node = nullptr;
};

class HDADevice : public Device, private IDeviceOperations, private ICharDeviceOperations
{
public:
	using Device::Device;
	virtual ~HDADevice() = default;

	IDeviceOperations& GetDeviceOperations() override
	{
		return *this;
	}

	ICharDeviceOperations* GetCharDeviceOperations() override
	{
		return this;
	}

	void SetHDAFunctions(IHDAFunctions& hdafunctions)
	{
		hdaFunctions = &hdafunctions;
	}

	Result Attach() override;
	Result Detach() override;

	void OnStreamIRQ(IHDAFunctions::Context ctx);
	Result IOControl(Process* proc, unsigned long req, void* buffer[]) override;

	Result Write(const void* buffer, size_t len, off_t offset) override;
	Result Read(void* buffer, size_t len, off_t offset) override;

	Result Open(Process* proc) override;
	Result Close(Process* proc) override;

protected:
	Result AttachNode(int cad, int nodeid);
	Result FillAWConnectionList(Node_AW& aw);
	Result AttachWidget_AudioOut(Node_AudioOut& ao);
	Result AttachWidget_AudioIn(Node_AudioIn& ai);
	Result AttachWidget_AudioMixer(Node_AudioMixer& am);
	Result AttachWidget_AudioSelector(Node_AudioSelector& as);
	Result AttachWidget_VendorDefined(Node_VendorDefined& vd);
	Result AttachWidget_PinComplex(Node_Pin& p);
	Result AttachNode_AFG(AFG& afg);
	Result AttachMultiPin_Render(AFG& afg, int association, int num_pins);
	Result AttachSinglePin_Render(AFG& afg, int association);
	Result AttachAFG(AFG& afg);

	Result RouteOutput(AFG& afg, int channels, Output& o, RoutingPlan** rp);

private:
	IHDAFunctions* hdaFunctions = nullptr;

	AFG* hda_afg;	/* XXX we support at most 1 AFG per device */
	PlayContext* hda_pc = nullptr;
};



void hda_stream_irq(device_t dev, void* ctx);
/* If true, we'll dump the entire HDA codec setup on attach */
#define HDA_VERBOSE 1

#if HDA_VERBOSE
# define HDA_VPRINTF(fmt,...) Printf(fmt, __VA_ARGS__)
#else
# define HDA_VPRINTF(...)
#endif

#if HDA_VERBOSE
const char* ResolveLocationToString(int location);
extern const char* const PinConnectionTypeString[];
extern const char* const PinDefaultDeviceString[];
extern const char* const PinConnectionTypeString[];
extern const char* const PinColorString[];
#endif

} // namespace hda

#endif /* __ANANAS_HDA_H__ */
