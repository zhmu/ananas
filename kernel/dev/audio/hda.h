#ifndef __ANANAS_HDA_H__
#define __ANANAS_HDA_H__

#include <ananas/types.h>
#include <ananas/util/list.h>
#include "kernel/device.h"
#include "verb.h"

class Result;

namespace hda
{
    struct PlayContext;

    class IHDAFunctions
    {
      public:
        typedef void* Context;
        virtual Result IssueVerb(uint32_t verb, uint32_t* resp, uint32_t* resp_ex) = 0;
        virtual uint16_t GetAndClearStateChange() = 0;
        virtual Result
        OpenStream(int tag, int dir, uint16_t fmt, int num_pages, Context* context) = 0;
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
        NT_VolumeKnob,
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
        NodeAddress(int cad, int nid) : na_cad(cad), na_nid(nid) {}

        int na_cad; /* Codec address of this node */
        int na_nid; /* Node ID */
    };

    // Base type of all HDA nodes
    class Node
    {
      public:
        Node(const NodeAddress& na) : n_address(na) {}

        constexpr uint32_t MakeVerb(uint32_t payload) const
        {
            return verb::Make(n_address.na_cad, n_address.na_nid, payload);
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
        Node_AW(const NodeAddress& na, uint32_t caps) : Node(na), aw_caps(caps) {}

        uint32_t aw_caps;            /* Audio widget capabilities */
        int aw_num_conn = 0;         /* Number of connections */
        Node_AW** aw_conn = nullptr; /* Input connection list */
    };

    class Node_Pin : public Node_AW
    {
      public:
        using Node_AW::Node_AW;
        HDANodeType GetType() const override { return NT_Pin; }

        uint32_t p_cap; /* Capabilities */
        uint32_t p_cfg; /* Pin configuration */
    };

    class Node_AudioOut : public Node_AW
    {
      public:
        using Node_AW::Node_AW;
        HDANodeType GetType() const override { return NT_AudioOut; }

        uint32_t ao_pcm; /* Output PCM capabilities */
    };

    class Node_AudioIn : public Node_AW
    {
      public:
        using Node_AW::Node_AW;
        HDANodeType GetType() const override { return NT_AudioIn; }
    };

    class Node_AudioMixer : public Node_AW
    {
      public:
        using Node_AW::Node_AW;
        HDANodeType GetType() const override { return NT_AudioMixer; }

        uint32_t am_ampcap_in;  /* Input amplifier capabilities */
        uint32_t am_ampcap_out; /* Output amplifier capabilities */
    };

    class Node_AudioSelector : public Node_AW
    {
      public:
        using Node_AW::Node_AW;
        HDANodeType GetType() const override { return NT_AudioSelector; }
    };

    class Node_VendorDefined : public Node_AW
    {
      public:
        using Node_AW::Node_AW;
        HDANodeType GetType() const override { return NT_VendorDefined; }
    };

    class Node_VolumeKnob : public Node_AW
    {
      public:
        using Node_AW::Node_AW;
        HDANodeType GetType() const override { return NT_VolumeKnob; }
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

        IDeviceOperations& GetDeviceOperations() override { return *this; }

        ICharDeviceOperations* GetCharDeviceOperations() override { return this; }

        void SetHDAFunctions(IHDAFunctions& hdafunctions) { hdaFunctions = &hdafunctions; }

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
        Result AttachWidget_VolumeKnob(Node_VolumeKnob& vk);
        Result AttachWidget_VendorDefined(Node_VendorDefined& vd);
        Result AttachWidget_PinComplex(Node_Pin& p);
        Result AttachNode_AFG(AFG& afg);
        Result AttachPin_Render(AFG& afg, int association, int num_pins);
        Result AttachAFG(AFG& afg);

        Result RouteOutput(AFG& afg, int channels, Output& o, RoutingPlan** rp);

      private:
        IHDAFunctions* hdaFunctions = nullptr;

        AFG* hda_afg; /* XXX we support at most 1 AFG per device */
        PlayContext* hda_pc = nullptr;
    };

    void hda_stream_irq(device_t dev, void* ctx);
/* If true, we'll dump the entire HDA codec setup on attach */
#define HDA_VERBOSE 1

#if HDA_VERBOSE
#define HDA_VPRINTF(fmt, ...) Printf(fmt, __VA_ARGS__)
#else
#define HDA_VPRINTF(...)
#endif

#if HDA_VERBOSE
    const char* ResolveLocationToString(int location);
    extern const char* const PinConnectionTypeString[];
    extern const char* const PinDefaultDeviceString[];
    extern const char* const PinConnectionTypeString[];
    extern const char* const PinColorString[];
#endif

#define HDF_DIR_IN 0
#define HDF_DIR_OUT 1

} // namespace hda

#endif /* __ANANAS_HDA_H__ */
