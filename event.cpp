#include "nssm.h"

#define NSSM_SOURCE _T("nssm")
#define NSSM_ERROR_BUFSIZE 65535
#define NSSM_NUM_EVENT_STRINGS 16
unsigned long tls_index;

/* Convert error code to error string - must call LocalFree() on return value */
TCHAR *error_string(unsigned long error) {
  /* Thread-safe buffer */
  TCHAR *error_message = (TCHAR *) TlsGetValue(tls_index);
  if (! error_message) {
    error_message = (TCHAR *) LocalAlloc(LPTR, NSSM_ERROR_BUFSIZE);
    if (! error_message) return _T("<out of memory for error message>");
    TlsSetValue(tls_index, (void *) error_message);
  }

  if (! FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 0, error, GetUserDefaultLangID(), (TCHAR *) error_message, NSSM_ERROR_BUFSIZE, 0)) {
    if (! FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 0, error, 0, (TCHAR *) error_message, NSSM_ERROR_BUFSIZE, 0)) {
      if (_sntprintf_s(error_message, NSSM_ERROR_BUFSIZE, _TRUNCATE, _T("system error %lu"), error) < 0) return 0;
    }
  }
  return error_message;
}

/* Convert message code to format string */
TCHAR *message_string(unsigned long error) {
  TCHAR *ret;
  if (! FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS, 0, error, GetUserDefaultLangID(), (LPTSTR) &ret, NSSM_ERROR_BUFSIZE, 0)) {
    if (! FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS, 0, error, 0, (LPTSTR) &ret, NSSM_ERROR_BUFSIZE, 0)) {
      ret = (TCHAR *) HeapAlloc(GetProcessHeap(), 0, 32 * sizeof(TCHAR));
      if (_sntprintf_s(ret, NSSM_ERROR_BUFSIZE, _TRUNCATE, _T("system error %lu"), error) < 0) return 0;
    }
  }
  return ret;
}

void log_event(unsigned short type, unsigned long id, ...) {
  va_list arg;
  TCHAR *strings[NSSM_NUM_EVENT_STRINGS];
  TCHAR *buffer = NULL;
  int len;

  TCHAR *format = message_string(id);
  if (!format) return;

  /* Open event log */
  HANDLE handle = RegisterEventSource(0, NSSM_SOURCE);
  if (! handle) return;

  /* Log it */
  va_start(arg, id);
  len = _vsctprintf_p(format, arg) + 1;
  buffer = (TCHAR*)malloc(len * sizeof(TCHAR));
  if (buffer)
  {
    _vstprintf_p(buffer, len, format, arg);
    strings[0] = buffer;
    strings[1] = 0;
    va_end(arg);
    ReportEvent(handle, type, 0, type, 0, 1, 0, (const TCHAR **) strings, 0);
    free(buffer);
  }

  /* Close event log */
  DeregisterEventSource(handle);
}

/* Log a message to the console */
void print_message(FILE *file, unsigned long id, ...) {
  va_list arg;

  TCHAR *format = message_string(id);
  if (! format) return;

  va_start(arg, id);
  _vftprintf(file, format, arg);
  va_end(arg);

  LocalFree(format);
}

/* Show a GUI dialogue */
int popup_message(HWND owner, unsigned int type, unsigned long id, ...) {
  va_list arg;

  TCHAR *format = message_string(id);
  if (! format) {
    return MessageBox(0, _T("The message which was supposed to go here is missing!"), NSSM, MB_OK | MB_ICONEXCLAMATION);
  }

  TCHAR blurb[NSSM_ERROR_BUFSIZE];
  va_start(arg, id);
  if (_vsntprintf_s(blurb, _countof(blurb), _TRUNCATE, format, arg) < 0) {
    va_end(arg);
    LocalFree(format);
    return MessageBox(0, _T("The message which was supposed to go here is too big!"), NSSM, MB_OK | MB_ICONEXCLAMATION);
  }
  va_end(arg);

  MSGBOXPARAMS params;
  ZeroMemory(&params, sizeof(params));
  params.cbSize = sizeof(params);
  params.hInstance = GetModuleHandle(0);
  params.hwndOwner = owner;
  params.lpszText = blurb;
  params.lpszCaption = NSSM;
  params.dwStyle = type;
  if (type == MB_OK) {
    params.dwStyle |= MB_USERICON;
    params.lpszIcon = MAKEINTRESOURCE(IDI_NSSM);
  }

  int ret = MessageBoxIndirect(&params);

  LocalFree(format);

  return ret;
}
