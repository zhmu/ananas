#ifndef __EFI_H__
#define __EFI_H__

#ifndef __i386__
#error Not supported
#endif

/* 2.3.1 Data types */
typedef unsigned char BOOLEAN;
typedef int INTN;
typedef unsigned int UINTN;
typedef char INT8;
typedef unsigned char UINT8;
typedef short INT16 ;
typedef unsigned short UINT16;
typedef int INT32;
typedef unsigned int UINT32;
typedef long long INT64;
typedef unsigned long long UINT64;
typedef char CHAR8;
typedef short CHAR16;
typedef void VOID;
typedef UINTN EFI_STATUS;
typedef VOID* EFI_HANDLE;
typedef VOID* EFI_EVENT;
typedef UINT64 EFI_LBA;
typedef UINTN EFI_TPL;

typedef struct {
	UINT32	Data1;
	UINT16	Data2;
	UINT16	Data3;
	UINT8	Data4[8];
} EFI_GUID;

#define IN
#define OUT
#define OPTIONAL
#define CONST const
#define EFIAPI


/* 4.2 EFI Table Header */
typedef struct {
	UINT64		Signature;
	UINT32		Revision;
	UINT32		HeaderSize;
	UINT32		CRC32;
	UINT32		Reserved;
} EFI_TABLE_HEADER;

/* 4.4 EFI Boot Services Table */
#define EFI_BOOT_SERVICES_SIGNATURE	0x56524553544f4f42
#define EFI_BOOT_SERVICES_REVISION	EFI_BOOT_SERVICES_SIGNATURE

typedef UINT64 EFI_PHYSICAL_ADDRESS;
typedef UINT64 EFI_VIRTUAL_ADDRESS;

typedef enum {
	AllocateAnyPages,
	AllocateMaxAddress,
	AllocateAddress,
	MaxAllocateType
} EFI_ALLOCATE_TYPE;

typedef enum {
	EfiReservedMemoryType,
	EfiLoaderCode,
	EfiLoaderData,
	EfiBootServicesCode,
	EfiBootServicesData,
	EfiRuntimeServicesCode,
	EfiRuntimeServicesData,
	EfiConventionalMemory,
	EfiUnusableMemory,
	EfiACPIReclaimMemory,
	EfiACPIMemoryNVS,
	EfiMemoryMappedIO,
	EfiMemoryMappedIOPortSpace,
	EfiPalCode,
	EfiMaxMemoryType
} EFI_MEMORY_TYPE;

#define EFI_MEMORY_DESCRIPTOR_VERSION	1
typedef struct {
	UINT32			Type;
	EFI_PHYSICAL_ADDRESS	PhysicalStart;
	EFI_VIRTUAL_ADDRESS	VirtualStart;
	UINT64			NumberOfPages;
	UINT64			Attribute;
#define EFI_MEMORY_UC		0x0000000000000001
#define EFI_MEMORY_WC		0x0000000000000002
#define EFI_MEMORY_WT		0x0000000000000004
#define EFI_MEMORY_WB		0x0000000000000008
#define EFI_MEMORY_UCE		0x0000000000000010
#define EFI_MEMORY_WP		0x0000000000001000
#define EFI_MEMORY_RP		0x0000000000002000
#define EFI_MEMORY_XP		0x0000000000004000
#define EFI_MEMORY_RUNTIME	0x8000000000000000
} EFI_MEMORY_DESCRIPTOR;

#define TPL_APPLICATION		4
#define TPL_CALLBACK		8
#define TPL_NOTIFY		16
#define TPL_HIGH_LEVEL		31

typedef EFI_STATUS (EFIAPI *EFI_RAISE_TPL) (
	IN EFI_TPL			NewTPL
);
typedef VOID (EFIAPI *EFI_RESTORE_TPL) (
	IN EFI_TPL			OldTPL
);
typedef EFI_STATUS (EFIAPI *EFI_ALLOCATE_PAGES) (
	IN EFI_ALLOCATE_TYPE		Type,
	IN EFI_MEMORY_TYPE		MemoryType,
	IN UINTN			Pages,
	IN OUT EFI_PHYSICAL_ADDRESS*	Memory
);
typedef EFI_STATUS (EFIAPI *EFI_FREE_PAGES) (
	IN EFI_PHYSICAL_ADDRESS		Memory,
	IN UINTN			Pages
);
typedef EFI_STATUS (EFIAPI *EFI_GET_MEMORY_MAP) (
	IN OUT UINTN*			MemoryMapSize,
	IN OUT EFI_MEMORY_DESCRIPTOR*	MemoryMap,
	OUT UINTN*			MemoryKey,
	OUT UINTN*			DescriptorSize,
	OUT UINT32*			DescriptorVersion
);
typedef EFI_STATUS (EFIAPI *EFI_ALLOCATE_POOL) (
	IN EFI_MEMORY_TYPE		PoolType,
	IN UINTN			Size,
	OUT VOID**			Buffer
);
typedef EFI_STATUS (EFIAPI *EFI_FREE_POOL) (
	IN VOID*			Buffer
);

/* Event */
#define EVT_TIMER				0x80000000
#define EVT_RUNTIME				0x40000000
#define EVT_NOTIFY_WAIT				0x00000100
#define EVT_NOTIFY_SIGNAL			0x00000200
#define EVT_SIGNAL_EXIT_BOOT_SERVICES		0x00000201
#define EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE	0x60000202

typedef enum {
	TimerCancel,
	TimerPeriodic,
	TimerRelative
} EFI_TIMER_DELAY;

typedef EFI_STATUS (EFIAPI *EFI_EVENT_NOTIFY) (
	IN EFI_EVENT			Event,
	IN VOID*			Context
);

typedef EFI_STATUS (EFIAPI *EFI_CREATE_EVENT) (
	IN UINT32			Type,
	IN EFI_TPL			NotifyTpl,
	IN EFI_EVENT_NOTIFY		NotifyFunction OPTIONAL,
	IN VOID*			NotifyContext OPTIONAL,
	OUT EFI_EVENT*			Event
);
typedef EFI_STATUS (EFIAPI *EFI_CREATE_EVENT_EX) (
	IN UINT32			Type,
	IN EFI_TPL			NotifyTpl,
	IN EFI_EVENT_NOTIFY		NotifyFunction OPTIONAL,
	IN CONST VOID*			NotifyContext OPTIONAL,
	IN CONST EFI_GUID*		EventGroup OPTIONAL,
	OUT EFI_EVENT*			Event
);

#define EFI_EVENT_GROUP_EXIT_BOOT_SERVICES \
	{ 0x27abf055, 0x0000b1b8, 0x00004c26, 0x00000080, 0x00000048, 0x00000074, \
	  0x0000008f, 0x00000037, 0x000000ba, 0x000000a2, 0x000000df }
#define EFI_EVENT_GROUP_VIRTUAL_ADDRESS_CHANGE \
	{ 0x13fa7698, 0x0000c831, 0x000049c7, 0x00000087, 0x000000ea, 0x0000008f, \
	  0x00000043, 0x000000fc, 0x000000c2, 0x00000051, 0x00000096 }
#define EFI_EVENT_GROUP_MEMORY_MAP_CHANGE \
	{ 0x78bee926, 0x0000692f, 0x000048fd, 0x0000009e, 0x000000db, 0x00000001, \
	  0x00000042, 0x0000002e, 0x000000f0, 0x000000d7, 0x000000ab }
#define EFI_EVENT_GROUP_READY_TO_BOOT \
	{ 0x7ce88fb3, 0x00004bd7, 0x00004679, 0x00000087, 0x000000a8, 0x000000a8, \
	  0x000000d8, 0x000000de, 0x000000e5, 0x0000000d, 0x0000002b }

typedef EFI_STATUS (EFIAPI *EFI_CLOSE_EVENT) (
	IN EFI_EVENT			Event
);
typedef EFI_STATUS (EFIAPI *EFI_SIGNAL_EVENT) (
	IN EFI_EVENT			Event
);
typedef EFI_STATUS (EFIAPI *EFI_WAIT_FOR_EVENT) (
	IN UINTN			NumberOfEvents,
	IN EFI_EVENT*			Event,
	OUT UINTN*			Index
);
typedef EFI_STATUS (EFIAPI *EFI_CHECK_EVENT) (
	IN EFI_EVENT			Event
);
typedef EFI_STATUS (EFIAPI *EFI_SET_TIMER) (
	IN EFI_EVENT			Event,
	IN EFI_TIMER_DELAY		Type,
	IN UINT64			TriggerTime
);

/* Driver Model Boot Services */
typedef enum {
	EFI_NATIVE_INTERFACE
} EFI_INTERFACE_TYPE;
typedef enum {
	AllHandles,
	ByRegisterNotify,
	ByProtocol
} EFI_LOCATE_SEARCH_TYPE;
typedef struct {
	EFI_HANDLE		AgentHandle;
	EFI_HANDLE		ControllerHandle;
	UINT32			Attributes;
	UINT32			OpenCount;
} EFI_OPEN_PROTOCOL_INFORMATION_ENTRY;
typedef struct {
	UINT8			Type;
	UINT8			SubType;
	UINT8			Length[2];
} EFI_DEVICE_PATH_PROTOCOL;

typedef EFI_STATUS (EFIAPI *EFI_INSTALL_PROTOCOL_INTERFACE) (
	IN OUT EFI_HANDLE*		Handle,
	IN EFI_GUID*			Protocol,
	IN EFI_INTERFACE_TYPE		InterfaceType,
	IN VOID*			Interface
);
typedef EFI_STATUS (EFIAPI *EFI_UNINSTALL_PROTOCOL_INTERFACE) (
	IN EFI_HANDLE*			Handle,
	IN EFI_GUID*			Protocol,
	IN VOID*			Interface
);
typedef EFI_STATUS (EFIAPI *EFI_REINSTALL_PROTOCOL_INTERFACE) (
	IN EFI_HANDLE*			Handle,
	IN EFI_GUID*			Protocol,
	IN VOID*			OldInterface,
	IN VOID*			NewInterface
);
typedef EFI_STATUS (EFIAPI *EFI_REGISTER_PROTOCOL_NOTIFY) (
	IN EFI_GUID*			Protocol,
	IN EFI_EVENT			Event,
	OUT VOID**			Registration
);
typedef EFI_STATUS (EFIAPI *EFI_LOCATE_HANDLE) (
	IN EFI_LOCATE_SEARCH_TYPE	SearchType,
	IN EFI_GUID*			Protocol OPTIONAL,
	IN VOID*			SearchKey OPTIONAL,
	IN OUT UINTN*			BufferSize,
	OUT EFI_HANDLE*			Buffer
);
typedef EFI_STATUS (EFIAPI *EFI_HANDLE_PROTOCOL) (
	IN EFI_HANDLE			Handle,
	IN EFI_GUID*			Protocol,
	OUT VOID**			Interface
);
typedef EFI_STATUS (EFIAPI *EFI_LOCATE_DEVICE_PATH) (
	IN EFI_GUID*			Protocol,
	IN OUT EFI_DEVICE_PATH_PROTOCOL** DevicePath,
	OUT EFI_HANDLE*			Device
);
typedef EFI_STATUS (EFIAPI *EFI_OPEN_PROTOCOL) (
	IN EFI_HANDLE			Handle,
	IN EFI_GUID*			Protocol,
	OUT VOID**			Interface OPTIONAL,
	IN EFI_HANDLE			AgentHandle,
	IN EFI_HANDLE			ControllerHandle,
	IN UINT32			Attributes
#define EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL	0x00000001
#define EFI_OPEN_PROTOCOL_GET_PROTOCOL		0x00000002
#define EFI_OPEN_PROTOCOL_TEST_PROTOCOL		0x00000004
#define EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER	0x00000008
#define EFI_OPEN_PROTOCOL_BY_DRIVER		0x00000010
#define EFI_OPEN_PROTOCOL_EXCLUSIVE		0x00000020
);
typedef EFI_STATUS (EFIAPI *EFI_CLOSE_PROTOCOL) (
	IN EFI_HANDLE			Handle,
	IN EFI_GUID*			Protocol,
	IN EFI_HANDLE			AgentHandle,
	IN EFI_HANDLE			ControllerHandle
);
typedef EFI_STATUS (EFIAPI *EFI_OPEN_PROTOCOL_INFORMATION) (
	IN EFI_HANDLE			Handle,
	IN EFI_GUID*			Protocol,
	OUT EFI_OPEN_PROTOCOL_INFORMATION_ENTRY** EntryBuffer,
	OUT UINTN*			EntryCount
);
typedef EFI_STATUS (EFIAPI *EFI_CONNECT_CONTROLLER) (
	IN EFI_HANDLE			ControllerHandle,
	IN EFI_HANDLE*			DriverImageHandle OPTIONAL,
	IN EFI_DEVICE_PATH_PROTOCOL*	RemainingDevicePath OPTIONAL,
	IN BOOLEAN			Recursive
);
typedef EFI_STATUS (EFIAPI *EFI_DISCONNECT_CONTROLLER) (
	IN EFI_HANDLE			ControllerHandle,
	IN EFI_HANDLE			DriverImageHandle OPTIONAL,
	IN EFI_HANDLE			ChildHandle OPTIONAL
);
typedef EFI_STATUS (EFIAPI *EFI_PROTOCOLS_PER_HANDLE) (
	IN EFI_HANDLE			Handle,
	OUT EFI_GUID**			ProtocolBuffer,
	OUT UINTN*			ProtocolBufferCount
);
typedef EFI_STATUS (EFIAPI *EFI_LOCATE_HANDLE_BUFFER) (
	IN EFI_LOCATE_SEARCH_TYPE	SearchType,
	IN EFI_GUID*			Protocol OPTIONAL,
	IN VOID*			SearchKey OPTIONAL,
	IN OUT UINTN*			NoHandles,
	OUT EFI_HANDLE**		Buffer
);
typedef EFI_STATUS (EFIAPI *EFI_LOCATE_PROTOCOL) (
	IN EFI_GUID*			Protocol,
	IN VOID*			Registration OPTIONAL,
	OUT VOID**			Interface
);
typedef EFI_STATUS (EFIAPI *EFI_INSTALL_MULTIPLE_PROTOCOL_INTERFACES) (
	IN OUT EFI_HANDLE*		Handle,
	...
);
typedef EFI_STATUS (EFIAPI *EFI_UNINSTALL_MULTIPLE_PROTOCOL_INTERFACES) (
	IN OUT EFI_HANDLE*		Handle,
	...
);

/* 6.4 Image services */
#define EFI_HII_PACKAGE_LIST_PROTOCOL_GUID \
	{ 0x6a1ee763, 0xd47a, 0x43b4, { 0xaa, 0xbe, 0xef, 0x1d, 0xe2, 0xab, 0x56, 0xfc } }

typedef EFI_STATUS (EFIAPI *EFI_LOAD_IMAGE) (
	IN BOOLEAN			BootPolicy,
	IN EFI_HANDLE			ParentImageHandle,
	IN EFI_DEVICE_PATH_PROTOCOL*	DevicePath,
	IN VOID*			SourceBuffer OPTIONAL,
	IN UINTN			SourceSize,
	OUT EFI_HANDLE*			ImageHandle
);
typedef EFI_STATUS (EFIAPI *EFI_START_IMAGE) (
	IN EFI_HANDLE			ImageHandle,
	OUT UINTN*			ExitDataSize,
	OUT CHAR16**			ExitData OPTIONAL
);
typedef EFI_STATUS (EFIAPI *EFI_UNLOAD_IMAGE) (
	IN EFI_HANDLE			ImageHandle
);
/*typedef EFI_STATUS (EFIAPI* EFI_IMAGE_ENTRY_POINT) (
	IN EFI_HANDLE			ImageHandle,
	IN EFI_SYSTEM_TABLE*		SystemTable
);*/
typedef EFI_STATUS (EFIAPI* EFI_EXIT) (
	IN EFI_HANDLE			ImageHandle,
	IN EFI_STATUS			ExitStatus,
	IN UINTN			ExitDataSize,
	IN CHAR16*			ExitData OPTIONAL
);
typedef EFI_STATUS (EFIAPI* EFI_EXIT_BOOT_SERVICES) (
	IN EFI_HANDLE			ImageHandle,
	IN UINTN			MapKey
);
typedef EFI_STATUS (EFIAPI* EFI_SET_WATCHDOG_TIMER) (
	IN UINTN			Timeout,
	IN UINT64			WatchdogCode,
	IN UINTN			DataSize,
	IN CHAR16*			WatchdogData OPTIONAL
);
typedef EFI_STATUS (EFIAPI* EFI_STALL) (
	IN UINTN			Microseconds
);
typedef VOID (EFIAPI* EFI_COPY_MEM) (
	IN VOID*			Destination,
	IN VOID*			Source,
	IN UINTN			Length
);
typedef VOID (EFIAPI* EFI_SET_MEM) (
	IN VOID*			Buffer,
	IN UINTN			Size,
	IN UINT8			Value
);
typedef EFI_STATUS (EFIAPI* EFI_GET_NEXT_MONOTONIC_COUNT) (
	OUT UINT64*			Count
);
typedef EFI_STATUS (EFIAPI* EFI_INSTALL_CONFIGURATION_TABLE) (
	IN EFI_GUID*			Guid,
	IN VOID*			Table
);
typedef EFI_STATUS (EFIAPI* EFI_CALCULATE_CRC32) (
	IN VOID*			Data,
	IN UINTN			DataSize,
	OUT UINT32*			Crc32
);

/* 7.2 Variable Services */
typedef EFI_STATUS (EFIAPI* EFI_GET_VARIABLE) (
	IN CHAR16*			VariableName,
	IN EFI_GUID*			VendorGuid,
	OUT UINT32*			Attributes OPTIONAL,
#define EFI_VARIABLE_NON_VOLATIL	E	0x00000001
#define EFI_VARIABLE_BOOTSERVICE_ACCESS		0x00000002
#define EFI_VARIABLE_RUNTIME_ACCESS		0x00000004
#define EFI_VARIABLE_HARDWARE_ERROR_RECORD	0x00000008
#define EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS	0x00000010
	IN OUT UINTN*			DataSize,
	OUT VOID*			Data
);
typedef EFI_STATUS (EFIAPI* EFI_GET_NEXT_VARIABLE_NAME) (
	IN OUT UINTN*			VariableNameSize,
	IN OUT CHAR16*			VariableName,
	IN OUT EFI_GUID*		VendorGuid
);
typedef EFI_STATUS (EFIAPI* EFI_SET_VARIABLE) (
	IN CHAR16*			VariableName,
	IN EFI_GUID*			VendorGuid,
	IN UINT32			Attributes,
	IN UINTN			DataSize,
	IN VOID*			Data
);
typedef EFI_STATUS (EFIAPI* EFI_QUERY_VARIABLE_INFO) (
	IN UINT32			Attributes,
	OUT UINT64*			MaximumVariableStorageSize,
	OUT UINT64*			RemainingVariableStorageSize,
	OUT UINT64*			MaximumVariableSize
);

/* 7.3 Time Services */
typedef struct {
	UINT16		Year;
	UINT8		Month;
	UINT8		Day;
	UINT8		Hour;
	UINT8		Minute;
	UINT8		Second;
	UINT8		Pad1;
	UINT32		Nanosecond;
	INT16		TimeZone;
	UINT8		Daylight;
	UINT8		Pad2;
} EFI_TIME;
typedef struct {
	UINT32		Resolution;
	UINT32		Accuracy;
	BOOLEAN		SetsToZero;
} EFI_TIME_CAPABILITIES;
typedef EFI_STATUS (EFIAPI* EFI_GET_TIME) (
	OUT EFI_TIME*			Time,
	OUT EFI_TIME_CAPABILITIES*	Capabilities OPTIONAL
);
typedef EFI_STATUS (EFIAPI* EFI_SET_TIME) (
	IN EFI_TIME*			Time
);
typedef EFI_STATUS (EFIAPI* EFI_GET_WAKEUP_TIME) (
	OUT BOOLEAN*			Enabled,
	OUT BOOLEAN*			Pending,
	OUT EFI_TIME*			Time
);
typedef EFI_STATUS (EFIAPI* EFI_SET_WAKEUP_TIME) (
	IN BOOLEAN			Enable,
	IN EFI_TIME*			Time OPTIONAL
);

/* 7.4 Virtual Memory Services */
typedef EFI_STATUS (EFIAPI* EFI_SET_VIRTUAL_ADDRESS_MAP) (
	IN UINTN			MemoryMapSize,
	IN UINTN			DescriptorSize,
	IN UINT32			DescriptorVersion,
	IN EFI_MEMORY_DESCRIPTOR*	VirtualMap
);
typedef EFI_STATUS (EFIAPI* EFI_CONVERT_POINTER) (
	IN UINTN			DebugDisposition,
#define EFI_OPTIONAL_PTR		0x00000001
	IN VOID**			Address
);

/* 7.5 Miscellaneous Runtime Services */
typedef enum {
	EfiResetCold,
	EfiResetWarm,
	EfiResetShutdown
} EFI_RESET_TYPE;
typedef struct {
	UINT64			Length;
	union {
		EFI_PHYSICAL_ADDRESS	DataBlock;
		EFI_PHYSICAL_ADDRESS	ContinuationPointer;
	} Union;
} EFI_CAPSULE_BLOCK_DESCRIPTION;
typedef struct {
	EFI_GUID		CapsuleGuid;
	UINT32			HeaderSize;
	UINT32			Flags;
#define CAPSULE_FLAGS_PERSIST_ACROSS_RESET	0x00010000
#define CAPSULE_FLAGS_POPULATE_SYSTEM_TABLE	0x00020000
#define CAPSULE_FLAGS_INITIATE_RESET		0x00040000
	UINT32			CapsuleImageSize;
} EFI_CAPSULE_HEADER;
typedef VOID (EFIAPI* EFI_RESET_SYSTEM) (
	IN EFI_RESET_TYPE		ResetType,
	IN EFI_STATUS			ResetStatus,
	IN UINTN			DataSize,
	IN VOID*			ResetData OPTIONAL
);
typedef EFI_STATUS (EFIAPI* EFI_GET_NEXT_HIGH_MONO_COUNT) (
	OUT UINT32*			HighCount
);
typedef EFI_STATUS (EFIAPI* EFI_UPDATE_CAPSULE) (
	IN EFI_CAPSULE_HEADER**		CapsuleHeaderArray,
	IN UINTN			CapsuleCount,
	IN EFI_PHYSICAL_ADDRESS		ScatterGatherList OPTIONAL
);
typedef EFI_STATUS (EFIAPI* EFI_QUERY_CAPSULE_CAPABILITIES) (
	IN EFI_CAPSULE_HEADER**		CapsuleHeaderArray,
	IN UINTN			CapsuleCount,
	OUT UINT64*			MaximumCapsuleSize,
	OUT EFI_RESET_TYPE*		ResetType
);

/* 11.3 Simple Text Input Protocol */
typedef struct {
	UINT16			ScanCode;
	CHAR16			UnicodeChar;
} EFI_INPUT_KEY;
typedef struct __EFI_SIMPLE_TEXT_INPUT_PROTOCOL EFI_SIMPLE_TEXT_INPUT_PROTOCOL;

typedef EFI_STATUS (EFIAPI* EFI_INPUT_RESET) (
	IN EFI_SIMPLE_TEXT_INPUT_PROTOCOL*	This,
	IN BOOLEAN				ExtendedVerification
);

typedef EFI_STATUS (EFIAPI* EFI_INPUT_READ_KEY) (
	IN EFI_SIMPLE_TEXT_INPUT_PROTOCOL*	This,
	OUT EFI_INPUT_KEY*			Key
);

struct __EFI_SIMPLE_TEXT_INPUT_PROTOCOL {
	EFI_INPUT_RESET			Reset;
	EFI_INPUT_READ_KEY		ReadKeyStroke;
	EFI_EVENT			WaitForKey;
};

/* 11.4 Simple Text Output Protocol */
typedef struct __EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
typedef struct {
	INT32				MaxMode;
	INT32				Mode;
	INT32				Attribute;
	INT32				CursorColumn;
	INT32				CursorRow;
	BOOLEAN				CursorVisible;
} SIMPLE_TEXT_OUTPUT_MODE;

typedef EFI_STATUS (EFIAPI* EFI_TEXT_RESET) (
	IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*	This,
	IN BOOLEAN				ExtendedVerification
);
typedef EFI_STATUS (EFIAPI* EFI_TEXT_STRING) (
	IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*	This,
	IN CHAR16*				String
);
typedef EFI_STATUS (EFIAPI* EFI_TEXT_TEST_STRING) (
	IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*	This,
	IN CHAR16*				String
);
typedef EFI_STATUS (EFIAPI* EFI_TEXT_QUERY_MODE) (
	IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*	This,
	IN UINTN				ModeNumber,
	OUT UINTN*				Columns,
	OUT UINTN*				Rows
);
typedef EFI_STATUS (EFIAPI* EFI_TEXT_SET_MODE) (
	IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*	This,
	IN UINTN				ModeNumber
);
typedef EFI_STATUS (EFIAPI* EFI_TEXT_SET_ATTRIBUTE) (
	IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*	This,
	IN UINTN				Attribute
);
typedef EFI_STATUS (EFIAPI* EFI_TEXT_CLEAR_SCREEN) (
	IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*	This
);
typedef EFI_STATUS (EFIAPI* EFI_TEXT_SET_CURSOR_POSITION) (
	IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*	This,
	IN UINTN				Column,
	IN UINTN				Row
);
typedef EFI_STATUS (EFIAPI* EFI_TEXT_ENABLE_CURSOR) (
	IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*	This,
	IN BOOLEAN				Visible
);

struct __EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
	EFI_TEXT_RESET			Reset;
	EFI_TEXT_STRING			OutputString;
	EFI_TEXT_TEST_STRING		TestString;
	EFI_TEXT_QUERY_MODE		QueryMode;
	EFI_TEXT_SET_MODE		SetMode;
	EFI_TEXT_SET_ATTRIBUTE		SetAttribute;
	EFI_TEXT_CLEAR_SCREEN		ClearScreen;
	EFI_TEXT_SET_CURSOR_POSITION	SetCursorPosition;
	EFI_TEXT_ENABLE_CURSOR		EnableCursor;
	SIMPLE_TEXT_OUTPUT_MODE*	Mode;
};

typedef struct {
	EFI_TABLE_HEADER			Hdr;
	// Task Priority Services
	EFI_RAISE_TPL				RaiseTPL;
	EFI_RESTORE_TPL				RestoreTPL;
	// Memory Services
	EFI_ALLOCATE_PAGES			AllocatePages;
	EFI_FREE_PAGES				FreePages;
	EFI_GET_MEMORY_MAP			GetMemoryMap;
	EFI_ALLOCATE_POOL			AllocatePool;
	EFI_FREE_POOL				FreePool;
	// Event & Timer Services
	EFI_CREATE_EVENT			CreateEvent;
	EFI_SET_TIMER				SetTimer;
	EFI_WAIT_FOR_EVENT			WaitForEvent;
	EFI_SIGNAL_EVENT			SignalEvent;
	EFI_CLOSE_EVENT				CloseEvent;
	EFI_CHECK_EVENT				CheckEvent;
	// Protocol Handler Services
	EFI_INSTALL_PROTOCOL_INTERFACE		InstallProtocolInterface;
	EFI_REINSTALL_PROTOCOL_INTERFACE	ReinstallProtocolInterface;
	EFI_UNINSTALL_PROTOCOL_INTERFACE	UninstallProtocolInterface;
	EFI_HANDLE_PROTOCOL			HandleProtocol;
	VOID*					Reserved;
	EFI_REGISTER_PROTOCOL_NOTIFY		RegisterProtocolNotify;
	EFI_LOCATE_HANDLE			LocateHandle;
	EFI_LOCATE_DEVICE_PATH			LocateDevicePath;
	EFI_INSTALL_CONFIGURATION_TABLE		InstallConfigurationTable;
	// Image Services
	EFI_LOAD_IMAGE				LoadImage;
	EFI_START_IMAGE				StartImage;
	EFI_EXIT				Exit;
	EFI_UNLOAD_IMAGE			UnloadImage;
	EFI_EXIT_BOOT_SERVICES			ExitBootServices;
	// Miscellaneous Services
	EFI_GET_NEXT_MONOTONIC_COUNT		GetNextMonotonicCount;
	EFI_STALL				Stall;
	EFI_SET_WATCHDOG_TIMER			SetWatchdogTimer;
	// DriverSupport Services
	EFI_CONNECT_CONTROLLER			ConnectController;
	EFI_DISCONNECT_CONTROLLER		DisconnectController;
	// Open and Close Protocol Services
	EFI_OPEN_PROTOCOL			OpenProtocol;
	EFI_CLOSE_PROTOCOL			CloseProtocol;
	EFI_OPEN_PROTOCOL_INFORMATION		OpenProtocolInformation;
	// Library Services
	EFI_PROTOCOLS_PER_HANDLE		ProtocolsPerHandle;
	EFI_LOCATE_HANDLE_BUFFER		LocateHandleBuffer;
	EFI_LOCATE_PROTOCOL			LocateProtocol;
	EFI_INSTALL_MULTIPLE_PROTOCOL_INTERFACES	InstallMultipleProtocolInterfaces;
	EFI_UNINSTALL_MULTIPLE_PROTOCOL_INTERFACES	UninstallMultipleProtocolInterfaces;
	// 32-bit CRC Services
	EFI_CALCULATE_CRC32			CalculateCrc32;
	// Miscellaneous Services
	EFI_COPY_MEM				CopyMem;
	EFI_SET_MEM				SetMem;
	EFI_CREATE_EVENT_EX			CreateEventEx;
	
} EFI_BOOT_SERVICES;

/* 4.5 EFI Runtime Services Table */
#define EFI_RUNTIME_SERVICES_SIGNATURE	0x56524553544e5552
#define EFI_RUNTIME_SERVICES_REVISION	EFI_RUNTIME_SERVICES_SIGNATURE

typedef struct {
	EFI_TABLE_HEADER		Hdr;
	// Time Services
	EFI_GET_TIME			GetTime;
	EFI_SET_TIME			SetTime;
	EFI_GET_WAKEUP_TIME		GetWakeupTime;
	EFI_SET_WAKEUP_TIME		SetWakeupTime;
	// Virtual Memory Services
	EFI_SET_VIRTUAL_ADDRESS_MAP	SetVirtualAddressMap;
	EFI_CONVERT_POINTER		ConvertPointer;
	// Variable Services
	EFI_GET_VARIABLE		GetVariable;
	EFI_GET_NEXT_VARIABLE_NAME	GetNextVariableName;
	EFI_SET_VARIABLE		SetVariable;
	// Miscellaneous Services
	EFI_GET_NEXT_HIGH_MONO_COUNT	GetNextHighMonotonicCount;
	EFI_RESET_SYSTEM		ResetSystem;
	// UEFI 2.0 Capsule Services
	EFI_UPDATE_CAPSULE		UpdateCapsule;
	EFI_QUERY_CAPSULE_CAPABILITIES	QueryCapsuleCapabilities;
	// Miscellaneous UEFI 2.0 Service
	EFI_QUERY_VARIABLE_INFO		QueryVariableInfo;
} EFI_RUNTIME_SERVICES;

/* 4.6 EFI Configuration Table */
typedef struct {
	EFI_GUID			VendorGuid;
	VOID*				VendorTable;
} EFI_CONFIGURATION_TABLE;

/* 4.3 */
#define EFI_SYSTEM_TABLE_SIGNATURE	0x5453595320494249
#define EFI_2_30_SYSTEM_TABLE_REVISION	((2<<16) | (30))
#define EFI_2_20_SYSTEM_TABLE_REVISION	((2<<16) | (20))
#define EFI_2_10_SYSTEM_TABLE_REVISION	((2<<16) | (10))
#define EFI_2_00_SYSTEM_TABLE_REVISION	((2<<16) | (00))
#define EFI_1_10_SYSTEM_TABLE_REVISION	((1<<16) | (10))
#define EFI_1_02_SYSTEM_TABLE_REVISION	((1<<16) | (02))
#define EFI_SYSTEM_TABLE_REVISION	EFI_2_30_SYSTEM_TABLE_REVISION
#define EFI_SPECIFICATION_VERSION	EFI_SYSTEM_TABLE_REVISION

typedef struct {
	EFI_TABLE_HEADER			Hdr;
	CHAR16*					FirmwareVendor;
	UINT32					FirmwareRevision;
	EFI_HANDLE				ConsoleInHandle;
	EFI_SIMPLE_TEXT_INPUT_PROTOCOL*		ConIn;
	EFI_HANDLE				ConsoleOutHandle;
	EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*	ConOut;
	EFI_HANDLE				StandardErrorHandle;;
	EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*	StdErr;
	EFI_RUNTIME_SERVICES*			RuntimeServices;
	EFI_BOOT_SERVICES*			BootServices;
	UINTN					NumberOfTableEntries;
	EFI_CONFIGURATION_TABLE*		ConfigurationTable;
} EFI_SYSTEM_TABLE;

#endif /* __EFI_H__ */
