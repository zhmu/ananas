/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __ANANAS_HDA_VERB_H__
#define __ANANAS_HDA_VERB_H__

namespace hda
{
    namespace verb
    {
        inline uint32_t MakePayload(uint32_t id, uint32_t data)
        {
            if (id < 16)
                return (id << 16) | data;
            else
                return (id << 8) | data;
        }

        inline constexpr uint32_t Make(uint32_t cad, uint32_t nodeid, uint32_t payload)
        {
            return (cad << 28) | (nodeid << 20) | payload;
        }

        constexpr int GetParam = 0xF00;
        namespace param
        {
            constexpr int VendorID = 0x00;
            namespace vendor_id
            {
                constexpr int VendorID(int x) { return x >> 16; }
                constexpr int DeviceID(int x) { return x & 0xffff; }
            } // namespace vendor_id
            constexpr int RevisionID = 0x02;
            namespace revision_id
            {
                constexpr int MajRev(int x) { return (x >> 20) & 15; }
                constexpr int MinRev(int x) { return (x >> 16) & 15; }
                constexpr int RevisionID(int x) { return (x >> 8) & 255; }
                constexpr int SteppingID(int x) { return x & 255; };
            } // namespace revision_id
            constexpr int SubNodeCount = 0x04;
            namespace sub_node_count
            {
                constexpr int Start(int x) { return (x >> 16) & 255; }
                constexpr int Total(int x) { return x & 255; }
            } // namespace sub_node_count
            constexpr int FGT = 0x05;
            namespace fgt
            {
                constexpr bool UnsolCapable(int x) { return ((x) >> 8) & 1; }
                constexpr bool NodeType(int x) { return x & 255; }
                namespace node_type
                {
                    constexpr int Audio = 1;
                    constexpr int Modem = 2;
                } // namespace node_type
            }     // namespace fgt
            constexpr int AGTCaps = 0x08;
            namespace agt_caps
            {
                constexpr bool BeepGen(int x) { return (x >> 16) & 1; }
                constexpr int InputDelay(int x) { return (x >> 8) & 15; }
                constexpr int OututDelay(int x) { return x & 15; }
            } // namespace agt_caps
            constexpr int AWCaps = 0x09;
            namespace aw_caps
            {
                constexpr int Type(int x) { return (x >> 20) & 15; }
                namespace type
                {
                    constexpr int AudioOut = 0;
                    constexpr int AudioIn = 1;
                    constexpr int AudioMixer = 2;
                    constexpr int AudioSelector = 3;
                    constexpr int AudioPinComplex = 4;
                    constexpr int AudioPower = 5;
                    constexpr int AudioVolumeKnob = 6;
                    constexpr int AudioBeepGenerator = 7;
                    constexpr int AudioVendorDefined = 15;
                } // namespace type
                constexpr int Delay(int x) { return (x >> 16) & 15; }
                constexpr int ChanCountEx(int x) { return (x >> 13) & 7; }
                constexpr bool CpCaps(int x) { return (x >> 12) & 1; }
                constexpr bool LRSwap(int x) { return (x >> 11) & 1; }
                constexpr bool PowerCntrl(int x) { return (x >> 10) & 1; }
                constexpr bool Digital(int x) { return (x >> 9) & 1; }
                constexpr bool ConnList(int x) { return (x >> 8) & 1; }
                constexpr bool UnSolCapable(int x) { return (x >> 7) & 1; }
                constexpr bool ProcWidget(int x) { return (x >> 6) & 1; }
                constexpr bool Stripe(int x) { return (x >> 5) & 1; }
                constexpr bool FormatOverride(int x) { return (x >> 4) & 1; }
                constexpr bool AmpParamOverride(int x) { return (x >> 3) & 1; }
                constexpr bool OutAmpPresent(int x) { return (x >> 2) & 1; }
                constexpr bool InAmpPresent(int x) { return (x >> 1) & 1; }
                constexpr int ChanCountLSB(int x) { return x & 1; }
            } // namespace aw_caps
            constexpr int PCM = 0xa;
            namespace pcm
            {
                constexpr bool B32(int x) { return (x >> 20) & 1; }
                constexpr bool B24(int x) { return (x >> 19) & 1; }
                constexpr bool B20(int x) { return (x >> 18) & 1; }
                constexpr bool B16(int x) { return (x >> 17) & 1; }
                constexpr bool B8(int x) { return (x >> 16) & 1; }
                constexpr bool R8_0(int x) { return x & 1; }
                constexpr bool R11_025(int x) { return (x >> 1) & 1; }
                constexpr bool R16_0(int x) { return (x >> 2) & 1; }
                constexpr bool R22_05(int x) { return (x >> 3) & 1; }
                constexpr bool R32_0(int x) { return (x >> 4) & 1; }
                constexpr bool R44_1(int x) { return (x >> 5) & 1; }
                constexpr bool R48_0(int x) { return (x >> 6) & 1; }
                constexpr bool R88_2(int x) { return (x >> 7) & 1; }
                constexpr bool R96_0(int x) { return (x >> 8) & 1; }
                constexpr bool R176_4(int x) { return (x >> 9) & 1; }
                constexpr bool R192_0(int x) { return (x >> 10) & 1; }
                constexpr bool R384_0(int x) { return (x >> 11) & 1; }
            } // namespace pcm
            constexpr int PinCap = 0x0c;
            namespace pin_cap
            {
                constexpr bool HBR(int x) { return (x >> 27) & 1; }
                constexpr bool DP(int x) { return (x >> 24) & 1; }
                constexpr bool EAPD(int x) { return (x >> 16) & 1; }
                constexpr int VRefControl(int x) { return (x >> 8) & 255; }
                constexpr bool HDMI(int x) { return (x >> 7) & 1; }
                constexpr bool BalancedIO(int x) { return (x >> 6) & 1; }
                constexpr bool Input(int x) { return (x >> 5) & 1; }
                constexpr bool Output(int x) { return (x >> 4) & 1; }
                constexpr bool Headphone(int x) { return (x >> 3) & 1; }
                constexpr bool Presence(int x) { return (x >> 2) & 1; }
                constexpr bool Trigger(int x) { return (x >> 1) & 1; }
                constexpr bool Impedance(int x) { return x & 1; }
            } // namespace pin_cap
            constexpr int AmpCap_In = 0x0d;
            constexpr int AmpCap_Out = 0x12;
            namespace amp_cap
            {
                constexpr bool Mute(int x) { return (x >> 31) & 1; }
                constexpr int StepSize(int x) { return (x >> 16) & 127; }
                constexpr int NumSteps(int x) { return (x >> 8) & 127; }
                constexpr int Offset(int x) { return x & 127; }
            } // namespace amp_cap
            constexpr int ConListLen = 0xe;
            namespace con_list_len
            {
                constexpr bool LongForm(int x) { return (x >> 7) & 1; }
                constexpr int Length(int x) { return x & 127; }
            } // namespace con_list_len
            constexpr int PowerStates = 0xf;
            namespace power_states
            {
                constexpr bool EPSS(int x) { return (x >> 31) & 1; }
                constexpr bool ClkStop(int x) { return (x >> 30) & 1; }
                constexpr bool S3D3ColdSup(int x) { return (x >> 29) & 1; }
                constexpr bool D3ColdSup(int x) { return (x >> 4) & 1; }
                constexpr bool D3CSup(int x) { return (x >> 3) & 1; }
                constexpr bool D2CSup(int x) { return (x >> 2) & 1; }
                constexpr bool D1CSup(int x) { return (x >> 1) & 1; }
                constexpr bool D0CSup(int x) { return x & 1; }
            } // namespace power_states
        }     // namespace param

        constexpr int GetConnSelect = 0xf01;
        constexpr int SetConnSelect = 0x701;
        constexpr int GetConnListEntry = 0xf02;

        constexpr int SetConvControl = 0x706;
        constexpr int GetConvControl = 0xf06;
        namespace conv_control
        {
            constexpr int Stream(int x) { return x << 4; }
            constexpr int Channel(int x) { return x; }
        } // namespace conv_control
        constexpr int GetPinSense = 0xf09;
        constexpr int SetPinControl = 0x707;
        constexpr int GetPinControl = 0xf07;
        namespace pin_control
        {
            constexpr int HEnable = (1 << 7);
            constexpr int OutEnable = (1 << 6);
            constexpr int InEnable = (1 << 5);
            constexpr int VRefEn(int x) { return x; }
        }; // namespace pin_control

        constexpr int SetConvFormat = 0x2;
        constexpr int GetConvFormat = 0xa;

        constexpr int SetAmpGainMute = 0x3;
        constexpr int GetAmpGainMute = 0xb;
        namespace amp_gain_mute
        {
            constexpr int Output = (1 << 15);
            constexpr int Input = (1 << 14);
            constexpr int Left = (1 << 13);
            constexpr int Right = (1 << 12);
            constexpr int Index(int x) { return x << 8; }
            constexpr int Mute = (1 << 7);
            constexpr int Gain(int x) { return x; }
        } // namespace amp_gain_mute

        constexpr int GetCfgDefault = 0xf1c;
        namespace cfg_default
        {
            constexpr int PortConnectivity(int x) { return (x >> 30) & 3; }
            namespace port_connectivity
            {
                constexpr int Jack = 0;
                constexpr int None = 1;
                constexpr int Fixed = 2;
                constexpr int Both = 3;
            } // namespace port_connectivity
            constexpr int Location(int x) { return (x >> 24) & 63; }
            constexpr int DefaultDevice(int x) { return (x >> 20) & 15; }
            namespace default_device
            {
                constexpr int LineOut = 0;
                constexpr int Speaker = 1;
                constexpr int HP_Out = 2;
                constexpr int CD = 3;
                constexpr int SPDIF_Out = 4;
                constexpr int Dig_Other_Out = 5;
                constexpr int ModemLineSide = 6;
                constexpr int ModemHandSetSide = 7;
                constexpr int LineIn = 8;
                constexpr int Aux = 9;
                constexpr int MicIn = 10;
                constexpr int Telephony = 11;
                constexpr int SPDIF_In = 12;
                constexpr int Digital_Other_In = 13;
                constexpr int Other = 15;
            } // namespace default_device
            constexpr int ConnectionType(int x) { return (x >> 16) & 15; }
            namespace connection_type
            {
                constexpr int Unknown = 0;
                constexpr int Stero_Mono_1_8 = 1;
                constexpr int Stero_Mono_1_4 = 2;
                constexpr int ATAPI = 3;
                constexpr int RCA = 4;
                constexpr int Optical = 5;
                constexpr int Other_Digital = 6;
                constexpr int Other_Analog = 7;
                constexpr int Multicannel_Analog = 8;
                constexpr int XLR = 9;
                constexpr int RJ11 = 10;
                constexpr int Combination = 11;
                constexpr int Other = 15;
            } // namespace connection_type
            constexpr int Color(int x) { return (x >> 12) & 15; }
            namespace color
            {
                constexpr int Unknown = 0;
                constexpr int Black = 1;
                constexpr int Grey = 2;
                constexpr int Blue = 3;
                constexpr int Green = 4;
                constexpr int Red = 5;
                constexpr int Orange = 6;
                constexpr int Yellow = 7;
                constexpr int Purple = 8;
                constexpr int Pink = 9;
                constexpr int White = 14;
                constexpr int Other = 15;
            } // namespace color
            constexpr int Misc(int x) { return (x >> 8) & 15; }
            namespace misc
            {
                constexpr bool NoPresenceDetect(int x) { return x & 1; }
            } // namespace misc
            constexpr int DefaultAssociation(int x) { return (x >> 4) & 15; }
            constexpr int Sequence(int x) { return x & 15; }
        } // namespace cfg_default

        constexpr int GetPowerState = 0xf05;
        constexpr int SetPowerState = 0x705;

        constexpr int GetVolumeKnobControl = 0xf0f;
        constexpr int SetVolumeKnobControl = 0x70f;

#define HDA_RESP_EX_CODEC_ADDR(x) ((x)&15)
#define HDA_RESP_EX_CODEC_UNSOLICITED(x) (((x) >> 4) & 1)

#define STREAM_TYPE_PCM (0 << 15)
#define STREAM_TYPE_NON_PCM (1 << 15)
#define STREAM_BASE_48_0 (0 << 14)
#define STREAM_BASE_44_1 (1 << 14)
#define STREAM_MULT_x1 (0 << 11)
#define STREAM_MULT_x2 (1 << 11)
#define STREAM_MULT_x3 (2 << 11)
#define STREAM_MULT_x4 (3 << 11)
#define STREAM_DIV_1 (0 << 8)
#define STREAM_DIV_2 (1 << 8)
#define STREAM_DIV_3 (2 << 8)
#define STREAM_DIV_4 (3 << 8)
#define STREAM_DIV_5 (4 << 8)
#define STREAM_DIV_6 (5 << 8)
#define STREAM_DIV_7 (6 << 8)
#define STREAM_DIV_8 (7 << 8)
#define STREAM_BITS_8 (0 << 4)
#define STREAM_BITS_16 (1 << 4)
#define STREAM_BITS_20 (2 << 4)
#define STREAM_BITS_24 (3 << 4)
#define STREAM_BITS_32 (4 << 4)
#define STREAM_CHANNELS(x) (x)

    } // namespace verb
} // namespace hda

#endif /* __ANANAS_HDA_VERB_H__ */
