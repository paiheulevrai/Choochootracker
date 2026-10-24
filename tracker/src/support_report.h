#ifndef __SUPPORT_REPORT_H__
#define __SUPPORT_REPORT_H__

int supportReportWrite(const char* path, const char* screenName);
int supportReportSaveDefault(const char* screenName, char* outputPath, int outputPathSize);

#endif
