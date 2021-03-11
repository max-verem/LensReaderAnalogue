#include <stdio.h>
#include <windows.h>

#include <quat.h>
#include <vrpn_Connection.h>
#include <vrpn_Analog.h>

#pragma comment(lib, "quatlib.lib")
#pragma comment(lib, "vrpn.lib")

HANDLE serial;

#define FOCUS_EXP_FILTER_RATIO 0.1
#define ZOOM_EXP_FILTER_RATIO 0.1
#define SENSORS_NUM 6
#define SENSORS_DEEP 6

#define TMP_BUF_SIZE 1024

typedef struct current_data_ctx
{
    CONDITION_VARIABLE cond;
    CRITICAL_SECTION lock;
    double vals[SENSORS_NUM][SENSORS_DEEP];
    struct timeval tv;
    int cnt;
} current_data_desc;

static current_data_ctx current_data;

static DWORD WINAPI reader_thread_proc(CONST LPVOID lpParam)
{
    long long tmp_buf_len = 0;
    char *tmp_buf = (char*)malloc(TMP_BUF_SIZE);
    DWORD sm = EV_RXCHAR | EV_ERR | EV_BREAK;

    memset(tmp_buf, 0, TMP_BUF_SIZE);

    /* clear port datas */
    PurgeComm(serial, PURGE_RXCLEAR | PURGE_TXCLEAR);

//    SetCommMask(serial, sm);

    /* read data */
    while (1)
    {
        int r;
        DWORD read_bytes = 0;

        /* wait for data in buffer */
        r = WaitCommEvent(serial, &sm, NULL);
        if (!(sm & EV_RXCHAR))
            continue;

        /* read byte */
        r = ReadFile
        (
            serial,                             /* handle to file               */
            tmp_buf + tmp_buf_len,              /* data buffer                  */
            TMP_BUF_SIZE - tmp_buf_len,         /* number of bytes to read      */
            &read_bytes,                        /* number of bytes read         */
            NULL                                /* overlapped buffer            */
        );
        if (read_bytes)
        {
            tmp_buf_len += read_bytes;

            /* search for \n */
            if (tmp_buf[0] != '\n')
            {
                char *p = strchr(tmp_buf, '\n');
                if (p)
                {
                    tmp_buf_len -= p - tmp_buf;
                    memmove(tmp_buf, p, tmp_buf_len);
                    memset(tmp_buf + tmp_buf_len, 0, TMP_BUF_SIZE - tmp_buf_len);
                }
            };

            /* search for \r */
            while (tmp_buf[0] == '\n' && tmp_buf_len)
            {
                char *p = strchr(tmp_buf, '\r');
                if (p)
                {
                    int r1, r2;
                    *p = 0; p++;
                    if (2 == sscanf(tmp_buf + 1, "%d/%d", &r1, &r2))
                    {
                        if (r1 > 0 && r2 > 0)
                        {
                            int i, j;

                            EnterCriticalSection(&current_data.lock);

                            /* shift sensors window */
                            for(i = 0; i < SENSORS_NUM; i++)
                            for (j = SENSORS_DEEP - 1; j > 0; j--)
                                current_data.vals[i][j] = current_data.vals[i][j - 1];

                            /* save current value */
                            current_data.vals[0][0] = current_data.vals[2][0] = current_data.vals[4][0] = r1;
                            current_data.vals[1][0] = current_data.vals[3][0] = current_data.vals[5][0] = r2;

                            /* do simple filtering for second pair */
#ifdef ZOOM_EXP_FILTER_RATIO
                            current_data.vals[2][0] = current_data.vals[4][0] * FOCUS_EXP_FILTER_RATIO + current_data.vals[2][1] * (1.0 - ZOOM_EXP_FILTER_RATIO);
#else
                            for (current_data.vals[2][0] = 0, j = 0; j < SENSORS_DEEP; j++)
                                current_data.vals[2][0] += current_data.vals[4][j];
                            current_data.vals[2][0] /= SENSORS_DEEP;
#endif
#ifdef FOCUS_EXP_FILTER_RATIO
                            current_data.vals[3][0] = current_data.vals[5][0] * ZOOM_EXP_FILTER_RATIO + current_data.vals[3][1] * (1.0 - FOCUS_EXP_FILTER_RATIO);
#else
                            for (current_data.vals[3][0] = 0, j = 0; j < SENSORS_DEEP; j++)
                                current_data.vals[3][0] += current_data.vals[5][j];
                            current_data.vals[3][0] /= SENSORS_DEEP;
#endif

                            current_data.cnt++;
                            vrpn_gettimeofday(&current_data.tv, NULL);
                            WakeConditionVariable(&current_data.cond);
                            LeaveCriticalSection(&current_data.lock);
                        };
                    };
                    tmp_buf_len -= p - tmp_buf;
                    memmove(tmp_buf, p, tmp_buf_len);
                    memset(tmp_buf + tmp_buf_len, 0, TMP_BUF_SIZE - tmp_buf_len);
                }
                else
                    break;
            };

            /* truncate buffer if overflow */
            if (tmp_buf_len > (TMP_BUF_SIZE))
                tmp_buf_len = 0;
        }
    }

    return 0;
}

#define VRPN_ANALOGUE_CHANNELS SENSORS_NUM

class LensReaderAnalogue : public vrpn_Analog
{
public:
    LensReaderAnalogue(vrpn_Connection *c) : vrpn_Analog("LensReaderAnalogue", c)
    {
        vrpn_Analog::num_channel = VRPN_ANALOGUE_CHANNELS;
        for (int i = 0; i < VRPN_ANALOGUE_CHANNELS; i++)
            vrpn_Analog::channel[i] = vrpn_Analog::last[i] = 0;
    }

    virtual ~LensReaderAnalogue()
    {
    }

    virtual void mainloop()
    {
        server_mainloop();
    }

    void update(double *sensor, struct timeval tv)
    {
        for (int i = 0; i < VRPN_ANALOGUE_CHANNELS; i++)
            vrpn_Analog::channel[i] = sensor[i];

        vrpn_Analog::timestamp = tv;

        vrpn_Analog::report_changes();
    }
};

int main(int argc, char** argv)
{
    int r;
    DCB dcb;
    COMMTIMEOUTS timeouts;
    char buf[128];
    HANDLE reader_thread;

    if (argc != 7)
    {
        fprintf(stderr, "Please specify input parameters:\n    <VRPN IP PORT> <SERIAL PORT> <raw zoom 1> <real focal length 1> <raw zoom 2> <real focal length 2>\n");
        exit(1);
    };

    // open serial port
    serial = CreateFile
    (
        argv[2],
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    if (INVALID_HANDLE_VALUE == serial)
    {
        char *msg;

        FormatMessage
        (
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
            NULL,
            GetLastError(),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR)&msg,
            0,
            NULL
        );

        fprintf(stderr, "ERROR: can't open serial device '%s': %s\n", argv[2], msg);

        exit(1);
    };

    /* serial port config */
    GetCommState(serial, &dcb);
    dcb.BaudRate = (UINT)CBR_38400;         // 38400 b/s
                                            // a. 1 start bit ( space )
    dcb.ByteSize = (BYTE)8;                 // b. 8 data bits
    dcb.Parity = (BYTE)ODDPARITY;           // c. 1 parity bit (odd)
    dcb.fParity = 1;
    dcb.StopBits = (BYTE)ONESTOPBIT;        // d. 1 stop bit (mark)
    dcb.fBinary = 1;
    dcb.fDtrControl = 0;
    dcb.fRtsControl = 0;
    dcb.fOutX = 0;
    dcb.fInX = 0;
    if(!SetCommState(serial, &dcb))
    {
        char *msg;

        FormatMessage
        (
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
            NULL,
            GetLastError(),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR)&msg,
            0,
            NULL
        );

        fprintf(stderr, "ERROR: can't setup serial device '%s': %s\n",  argv[2], msg);

        exit(1);
    }

    // set short timeouts on the comm port.
    timeouts.ReadIntervalTimeout = 1;
    timeouts.ReadTotalTimeoutMultiplier = 1;
    timeouts.ReadTotalTimeoutConstant = 1;
    timeouts.WriteTotalTimeoutMultiplier = 1;
    timeouts.WriteTotalTimeoutConstant = 1;
    if (!SetCommTimeouts(serial, &timeouts))
    {
        char *msg;

        FormatMessage
        (
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
            NULL,
            GetLastError(),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR)&msg,
            0,
            NULL
        );

        fprintf(stderr, "ERROR: can't setup serial device '%s': %s\n", argv[2], msg);

        exit(1);
    }

    /* setup conditional variable */
    InitializeConditionVariable(&current_data.cond);
    InitializeCriticalSection(&current_data.lock);

    /* create VRPN connection */
    snprintf(buf, sizeof(buf), ":%s", argv[1]);
    vrpn_Connection *conn = vrpn_create_server_connection(buf);

    /* start thread */
    reader_thread = CreateThread
    (
        NULL,               /* LPSECURITY_ATTRIBUTES lpThreadAttributes,*/
        8096,               /* SIZE_T dwStackSize,                      */
        reader_thread_proc, /* LPTHREAD_START_ROUTINE lpStartAddress,   */
        NULL,               /* LPVOID lpParameter,                      */
        0,                  /* DWORD dwCreationFlags,                   */
        NULL                /* LPDWORD lpThreadId                       */
    );

    // Creating the network server
    LensReaderAnalogue *vrpn = new LensReaderAnalogue(conn);

    EnterCriticalSection(&current_data.lock);
    while (1)
    {
        struct timeval tv;
        double
            sensor[4],
            zoom_raw_1 = atof(argv[3]),
            zoom_real_1 = atof(argv[4]),
            zoom_raw_2 = atof(argv[5]),
            zoom_real_2 = atof(argv[6]);

        if(!current_data.cnt)
            SleepConditionVariableCS(&current_data.cond, &current_data.lock, 100);

        if (r = current_data.cnt)
        {
            /* first pair is recalculated value */
            sensor[0] = zoom_real_1 + (zoom_real_2 - zoom_real_1) / (zoom_raw_2 - zoom_raw_1) * (current_data.vals[2][0] - zoom_raw_1);
            sensor[1] = current_data.vals[3][0];

            /* second pair - filtered raw values */
            sensor[2] = current_data.vals[2][0];
            sensor[3] = current_data.vals[3][0];

            /* third pair - original raw values */
            sensor[4] = current_data.vals[4][0];
            sensor[5] = current_data.vals[5][0];

            vrpn->update(sensor, tv = current_data.tv);

            current_data.cnt = 0;
        };

        LeaveCriticalSection(&current_data.lock);

        vrpn->mainloop();
        conn->mainloop();

        if (r)
            printf("%8d.%04d | Zoom: trg=%5.4f flt=%5.4f raw=%5.4f | Focus: trg=%5.4f flt=%5.4f raw=%5.4f\r",
                tv.tv_sec, tv.tv_usec / 1000,
                sensor[0], sensor[2], sensor[4],
                sensor[1], sensor[3], sensor[5]);

        EnterCriticalSection(&current_data.lock);
    }

    return 0;
}
