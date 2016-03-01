#ifndef _INCLUDE_DEMORECORDER_H
#define _INCLUDE_DEMORECORDER_H


class CDemoFile;
class bf_read;
class ServerClass;
class CGameInfo;

class IDemoRecorder
{
public:

	virtual CDemoFile *GetDemoFile() = 0;
	virtual int		GetRecordingTick(void) = 0;

	virtual void	StartRecording(const char *filename, bool bContinuously) = 0;
	virtual void	SetSignonState(int state) = 0;
	virtual bool	IsRecording(void) = 0;
	virtual void	PauseRecording(void) = 0;
	virtual void	ResumeRecording(void) = 0;
#if SOURCE_ENGINE == SE_CSGO
	virtual void	StopRecording(CGameInfo const *info) = 0;
#else
	virtual void	StopRecording(void) = 0;
#endif

	virtual void	RecordCommand(const char *cmdstring) = 0; 
	virtual void	RecordUserInput(int cmdnumber) = 0;
	virtual void	RecordMessages(bf_read &data, int bits) = 0;
	virtual void	RecordPacket(void) = 0;
	virtual void	RecordServerClasses(ServerClass *pClasses) = 0;
	virtual void	RecordStringTables(void);
#if SOURCE_ENGINE == SE_CSGO
	virtual void	RecordCustomData(int, void const *, unsigned int);
#endif

	virtual void	ResetDemoInterpolation(void) = 0;
};
#endif
