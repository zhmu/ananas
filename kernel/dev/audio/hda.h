/* If true, we'll dump the entire HDA codec setup on attach */
#define HDA_VERBOSE 1

#if HDA_VERBOSE
# define HDA_VPRINTF(fmt,...) device_printf(dev, fmt, __VA_ARGS__)
#else
# define HDA_VPRINTF(...)
#endif

/* hda-codec.c */
errorcode_t hda_attach_node(device_t dev, int cad, int nodeid);
#if HDA_VERBOSE
const char* hda_resolve_location(int location);
extern const char* const hda_pin_connectivity_string[];
extern const char* const hda_pin_default_device_string[];
extern const char* const hda_pin_connection_type_string[];
extern const char* const hda_pin_color_string[];
#endif

/* hda-route.c */
errorcode_t hda_route_output(device_t dev, struct HDA_AFG* afg, int channels, struct HDA_OUTPUT* o, struct HDA_ROUTING_PLAN** rp);
