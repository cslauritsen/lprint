//
// Header file for LPrint, a Label Printer Application
//
// Copyright © 2019-2020 by Michael R Sweet.
// Copyright © 2010-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _LPRINT_H_
#  define _LPRINT_H_

//
// Include necessary headers...
//

#  include "device.h"
#  include "driver.h"

#  include <limits.h>
#  include <poll.h>
#  include <sys/fcntl.h>
#  include <sys/stat.h>
#  include <sys/wait.h>

extern char **environ;

#  ifdef HAVE_DNSSD
#    include <dns_sd.h>
#  elif defined(HAVE_AVAHI)
#    include <avahi-client/client.h>
#    include <avahi-client/publish.h>
#    include <avahi-common/error.h>
#    include <avahi-common/thread-watch.h>
#  endif // HAVE_DNSSD


//
// New operations/tags defined in CUPS 2.3 and later...
//

#  if CUPS_VERSION_MAJOR == 2 && CUPS_VERSION_MINOR < 3
#    define IPP_OP_CREATE_PRINTER		(ipp_op_t)0x004C
#    define IPP_OP_DELETE_PRINTER		(ipp_op_t)0x004E
#    define IPP_OP_GET_PRINTERS			(ipp_op_t)0x004F
#    define IPP_OP_GET_SYSTEM_ATTRIBUTES	(ipp_op_t)0x005B
#    define IPP_OP_SET_SYSTEM_ATTRIBUTES	(ipp_op_t)0x0062
#    define IPP_OP_SHUTDOWN_ALL_PRINTERS	(ipp_op_t)0x0063

#    define IPP_TAG_SYSTEM			(ipp_tag_t)0x000A
#  endif // CUPS_VERSION_MAJOR == 2 && CUPS_VERSION_MINOR < 3


//
// Wrappers for global constants...
//

#  ifdef _LPRINT_C_
#    define VAR
#    define VALUE(...)	= __VA_ARGS__
#  else
#    define VAR		extern
#    define VALUE(...)
#  endif // _LPRINT_C_


//
// Constants...
//

typedef enum lprint_loglevel_e		// Log levels
{
  LPRINT_LOGLEVEL_UNSPEC = -1,		// Not specified
  LPRINT_LOGLEVEL_DEBUG,		// Debug message
  LPRINT_LOGLEVEL_INFO,			// Informational message
  LPRINT_LOGLEVEL_WARN,			// Warning message
  LPRINT_LOGLEVEL_ERROR,		// Error message
  LPRINT_LOGLEVEL_FATAL			// Fatal message
} lprint_loglevel_t;

enum lprint_preason_e			// printer-state-reasons bit values
{
  LPRINT_PREASON_NONE = 0x0000,		// none
  LPRINT_PREASON_OTHER = 0x0001,	// other
  LPRINT_PREASON_COVER_OPEN = 0x0002,	// cover-open
  LPRINT_PREASON_MEDIA_EMPTY = 0x0004,	// media-empty
  LPRINT_PREASON_MEDIA_JAM = 0x0008,	// media-jam
  LPRINT_PREASON_MEDIA_LOW = 0x0010,	// media-low
  LPRINT_PREASON_MEDIA_NEEDED = 0x0020	// media-needed
};
typedef unsigned int lprint_preason_t;	// Bitfield for printer-state-reasons
VAR const char * const lprint_preason_strings[6]
VALUE({					// Strings for each bit
  // "none" is implied for no bits set
  "other",
  "cover-open",
  "media-empty",
  "media-jam",
  "media-low",
  "media-needed"
});


//
// Types and structures...
//

#  ifdef HAVE_DNSSD
typedef DNSServiceRef lprint_srv_t;	// Service reference
typedef TXTRecordRef lprint_txt_t;	// TXT record

#elif defined(HAVE_AVAHI)
typedef AvahiEntryGroup *lprint_srv_t;	// Service reference
typedef AvahiStringList *lprint_txt_t;	// TXT record

#else
typedef void *lprint_srv_t;		// Service reference
typedef void *lprint_txt_t;		// TXT record
#endif // HAVE_DNSSD

typedef struct lprint_filter_s		// Attribute filter
{
  cups_array_t		*ra;		// Requested attributes
  ipp_tag_t		group_tag;	// Group to copy
} lprint_filter_t;

typedef struct lprint_system_s		// System data
{
  pthread_rwlock_t	rwlock;		// Reader/writer lock
  time_t		start_time,	// Startup time
			clean_time,	// Next clean time
			save_time,	// Do we need to save the config?
			shutdown_time;	// Shutdown requested?
  char			*hostname;	// Hostname
  int			port;		// Port number, if any
  char			*directory;	// Spool directory
  char			*logfile;	// Log filename, if any
  int			logfd;		// Log file descriptor, if any
  lprint_loglevel_t	loglevel;	// Log level
  char			*subtypes;	// DNS-SD sub-types, if any
  char			*auth_service;	// PAM authorization service, if any
  char			*admin_group;	// PAM administrative group, if any
  gid_t			admin_gid;	// PAM administrative group ID
  char			*session_key;	// Session key
  int			num_listeners;	// Number of listener sockets
  struct pollfd		listeners[3];	// Listener sockets
  int			next_client;	// Next client number
  cups_array_t		*printers;	// Array of printers
  int			default_printer,// Default printer-id
			next_printer_id;// Next printer-id
} lprint_system_t;

struct lprint_printer_s			// Printer data
{
  pthread_rwlock_t	rwlock;		// Reader/writer lock
  lprint_system_t	*system;	// Containing system
#  ifdef HAVE_DNSSD
  lprint_srv_t		ipp_ref,	// DNS-SD IPP service
			ipps_ref,	// DNS-SD IPPS service
			http_ref,	// DNS-SD HTTP service
			printer_ref;	// DNS-SD LPD service
#  elif defined(HAVE_AVAHI)
  lprint_srv_t		dnssd_ref;	// DNS-SD services
#  endif // HAVE_DNSSD
  int			printer_id;	// printer-id
  char			*printer_name,	// printer-name
			*dns_sd_name,	// printer-dns-sd-name
			*location,	// Human-readable location
			*geo_location,	// Geographic location (geo: URI)
			*organization,	// Organization
			*org_unit,	// Organizational unit
			*resource;	// Resource path of printer
  size_t		resourcelen;	// Length of resource path
  char			*device_uri,	// Device URI
			*driver_name;	// Driver name
  lprint_driver_t	*driver;	// Driver
  ipp_t			*attrs;		// Static attributes
  ipp_attribute_t	*xri_supported;	// printer-xri-supported attribute
  time_t		start_time;	// Startup time
  time_t		config_time;	// printer-config-change-time
  ipp_pstate_t		state;		// printer-state value
  lprint_preason_t	state_reasons;	// printer-state-reasons values
  time_t		state_time;	// printer-state-change-time
  time_t		status_time;	// Last time status was updated
  lprint_job_t		*processing_job;// Currently printing job, if any
  cups_array_t		*active_jobs,	// Array of active jobs
			*completed_jobs,// Array of completed jobs
			*jobs;		// Array of all jobs
  int			next_job_id,	// Next job-id
			is_deleted,	// Non-zero if deleted
			impcompleted;	// printer-impressions-completed
};

struct lprint_job_s			// Job data
{
  pthread_rwlock_t	rwlock;		// Reader/writer lock
  lprint_system_t	*system;	// Containing system
  lprint_printer_t	*printer;	// Printer
  int			id;		// Job ID
  const char		*name,		// job-name
			*username,	// job-originating-user-name
			*format;	// document-format
  ipp_jstate_t		state;		// job-state value
  char			*message;	// job-state-message value
  int			msglevel;	// job-state-message log level (0=error, 1=info)
  time_t		created,	// [date-]time-at-creation value
			processing,	// [date-]time-at-processing value
			completed;	// [date-]time-at-completed value
  int			impressions,	// job-impressions value
			impcompleted;	// job-impressions-completed value
  ipp_t			*attrs;		// Static attributes
  int			cancel;		// Non-zero when job canceled
  char			*filename;	// Print file name
  int			fd;		// Print file descriptor
};

typedef struct lprint_client_s		// Client data
{
  lprint_system_t	*system;	// Containing system
  int			number;		// Connection number
  pthread_t		thread_id;	// Thread ID
  http_t		*http;		// HTTP connection
  ipp_t			*request,	// IPP request
			*response;	// IPP response
  time_t		start;		// Request start time
  http_state_t		operation;	// Request operation
  ipp_op_t		operation_id;	// IPP operation-id
  char			uri[1024],	// Request URI
			*options;	// URI options
  http_addr_t		addr;		// Client address
  char			hostname[256];	// Client hostname
  char			username[256];	// Authenticated username, if any
  lprint_printer_t	*printer;	// Printer, if any
  lprint_job_t		*job;		// Job, if any
} lprint_client_t;


//
// Functions...
//

extern void		lprintAddOptions(ipp_t *request, int num_options, cups_option_t *options);
extern void		lprintAddPrinterURI(ipp_t *request, const char *printer_name, char *resource, size_t rsize);
extern void		lprintCheckJobs(lprint_printer_t *printer);
extern void		lprintCleanJobs(lprint_system_t *system);
extern http_t		*lprintConnect(int auto_start);
extern http_t		*lprintConnectURI(const char *printer_uri, char *resource, size_t rsize);
extern void		lprintCopyAttributes(ipp_t *to, ipp_t *from, cups_array_t *ra, ipp_tag_t group_tag, int quickcopy);
extern lprint_client_t	*lprintCreateClient(lprint_system_t *system, int sock);
extern lprint_job_t	*lprintCreateJob(lprint_client_t *client);
extern int		lprintCreateJobFile(lprint_job_t *job, char *fname, size_t fnamesize, const char *dir, const char *ext);
extern lprint_printer_t	*lprintCreatePrinter(lprint_system_t *system, int printer_id, const char *printer_name, const char *driver_name, const char *device_uri, const char *location, const char *geo_location, const char *organization, const char *org_unit);
extern lprint_system_t	*lprintCreateSystem(const char *hostname, int port, const char *subtypes, const char *spooldir, const char *logfile, lprint_loglevel_t loglevel, const char *auth_service, const char *admin_group);
extern void		lprintDeleteClient(lprint_client_t *client);
extern void		lprintDeleteJob(lprint_job_t *job);
extern void		lprintDeletePrinter(lprint_printer_t *printer);
extern void		lprintDeleteSystem(lprint_system_t *system);
extern int		lprintDoAdd(int num_options, cups_option_t *options);
extern int		lprintDoCancel(int num_options, cups_option_t *options);
extern int		lprintDoDefault(int num_options, cups_option_t *options);
extern int		lprintDoDelete(int num_options, cups_option_t *options);
extern int		lprintDoDevices(int num_options, cups_option_t *options);
extern int		lprintDoDrivers(int num_options, cups_option_t *options);
extern int		lprintDoJobs(int num_options, cups_option_t *options);
extern int		lprintDoModify(int num_options, cups_option_t *options);
extern int		lprintDoOptions(int num_options, cups_option_t *options);
extern int		lprintDoPrinters(int num_options, cups_option_t *options);
extern int		lprintDoServer(int num_options, cups_option_t *options);
extern int		lprintDoShutdown(int num_options, cups_option_t *options);
extern int		lprintDoStatus(int num_options, cups_option_t *options);
extern int		lprintDoSubmit(int num_files, char **files, int num_options, cups_option_t *options);
extern lprint_job_t	*lprintFindJob(lprint_printer_t *printer, int job_id);
extern lprint_printer_t	*lprintFindPrinter(lprint_system_t *system, const char *resource, int printer_id);
extern char		*lprintGetDefaultPrinter(http_t *http, char *buffer, size_t bufsize);
extern char		*lprintGetServerPath(char *buffer, size_t bufsize);
extern void		lprintInitDNSSD(lprint_system_t *system);
extern http_status_t	lprintIsAuthorized(lprint_client_t *client);
// Note: Log functions currently only support %d, %p, %s, %u, and %x!
extern void		lprintLog(lprint_system_t *system, lprint_loglevel_t level, const char *message, ...);
extern void		lprintLogAttributes(lprint_client_t *client, const char *title, ipp_t *ipp, int is_response);
extern void		lprintLogClient(lprint_client_t *client, lprint_loglevel_t level, const char *message, ...) LPRINT_FORMAT(3, 4);
extern void		lprintLogJob(lprint_job_t *job, lprint_loglevel_t level, const char *message, ...) LPRINT_FORMAT(3, 4);
extern void		lprintLogPrinter(lprint_printer_t *printer, lprint_loglevel_t level, const char *message, ...) LPRINT_FORMAT(3, 4);
extern char		*lprintMakeUUID(lprint_system_t *system, const char *printer_name, int job_id, char *buffer, size_t bufsize);
extern void		*lprintProcessClient(lprint_client_t *client);
extern int		lprintProcessHTTP(lprint_client_t *client);
extern int		lprintProcessIPP(lprint_client_t *client);
extern void		*lprintProcessJob(lprint_job_t *job);
extern unsigned		lprintRand(void);
extern int		lprintRegisterDNSSD(lprint_printer_t *printer);
extern int		lprintRespondHTTP(lprint_client_t *client, http_status_t code, const char *content_coding, const char *type, size_t length);
extern void		lprintRespondIPP(lprint_client_t *client, ipp_status_t status, const char *message, ...) LPRINT_FORMAT(3, 4);
extern void		lprintRunSystem(lprint_system_t *system);
extern void		lprintUnregisterDNSSD(lprint_printer_t *printer);

#endif // !_LPRINT_H_
