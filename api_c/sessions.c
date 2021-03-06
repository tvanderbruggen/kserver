/** 
 * Implementation of sessions.h
 *
 * (c) Koheron
 */
 
#include "sessions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Fields of the session status */
enum session_fields {
    SESS_FIELD_NONE,
    SESS_FIELD_ID,
    SESS_FIELD_CONN_TYPE,
    SESS_FIELD_CLIENT_IP,
    SESS_FIELD_CLIENT_PORT,
    SESS_FIELD_REQ_NUM,
    SESS_FIELD_ERR_NUM,
    SESS_FIELD_UPTIME,
    SESS_FIELD_PERMISSIONS,
    session_fields_num
};

/**
 * __append_session - Add a session status to the running sessions
 * @sessions: Running sessions to extend
 * @sess_status: Session status to append 
 */
static void __append_session(struct running_sessions *sessions, 
                             const struct session_status *sess_status)
{
    sessions->sessions[sessions->sess_num].sess_id = sess_status->sess_id;
    sessions->sessions[sessions->sess_num].conn_type = sess_status->conn_type;
    strcpy(sessions->sessions[sessions->sess_num].clt_ip, sess_status->clt_ip);
    sessions->sessions[sessions->sess_num].clt_port = sess_status->clt_port;
    sessions->sessions[sessions->sess_num].req_num = sess_status->req_num;
    sessions->sessions[sessions->sess_num].error_num = sess_status->error_num;
    sessions->sessions[sessions->sess_num].uptime = sess_status->uptime;
    strcpy(sessions->sessions[sessions->sess_num].permissions, sess_status->permissions);
    
    sessions->sess_num++;
}

/**
 * __get_sessions_data - Load running sessions data
 * @kcl: Kclient structure
 * @rcv_buffer: Reception buffer to load
 *
 * Returns the number of bytes received on success, -1 if failure
 */
static int __get_sessions_data(struct kclient *kcl, struct rcv_buff *rcv_buffer)
{
    int bytes_read;
    
    if (kclient_send_string(kcl, "1|4|\n") < 0)
        return -1;
    
    bytes_read = kclient_rcv_esc_seq(kcl, rcv_buffer, "EORS");
    
    if (bytes_read < 0)
        return -1;
        
    printf("bytes_read = %u\n", bytes_read);
    printf("%s\n", rcv_buffer->buffer);
    
    return bytes_read;
}

struct running_sessions* kclient_get_running_sessions(struct kclient *kcl)
{
    int i, tmp_buff_cnt;
    int current_field = SESS_FIELD_ID;
    struct running_sessions *sessions;
    struct session_status tmp_session;
    char tmp_buff[2048];
    
    struct rcv_buff rcv_buffer;
    char *buffer = rcv_buffer.buffer;
    int bytes_read = __get_sessions_data(kcl, &rcv_buffer);
    
    if (bytes_read < 0) {
        fprintf(stderr, "Can't get running session data\n");
        return NULL;
    }
    
    sessions = malloc(sizeof *sessions);
    
    if (sessions == NULL) {
        fprintf(stderr, "Can't allocate sessions memory\n");
        return NULL;
    }
    
    // Parse rcv_buffer
    tmp_buff[0] = '\0';
    tmp_buff_cnt = 0;
    sessions->sess_num = 0;
    
    for (i=0; i<bytes_read; i++) {
        if (buffer[i] == ':') {
            tmp_buff[tmp_buff_cnt] = '\0';
            
            switch (current_field) {
              case SESS_FIELD_ID:
                tmp_session.sess_id
                    = (int) strtol(tmp_buff, (char **)NULL, 10);

                current_field++;
                break;
              case SESS_FIELD_CONN_TYPE:
                if (strcmp(tmp_buff, "TCP") == 0) {
                    tmp_session.conn_type = TCP;
                } 
                else if (strcmp(tmp_buff, "WEBSOCK") == 0) {
                    tmp_session.conn_type = WEBSOCK;
                }
                else if (strcmp(tmp_buff, "UNIX") == 0) {
                    tmp_session.conn_type = UNIX;
                } else {
                    fprintf(stderr, "Invalid connection type\n");
                    return NULL;
                }

                current_field++;
                break;
              case SESS_FIELD_CLIENT_IP:
                strcpy(tmp_session.clt_ip, tmp_buff);
                current_field++;
                break;
              case SESS_FIELD_CLIENT_PORT:
                tmp_session.clt_port
                    = (int) strtol(tmp_buff, (char **)NULL, 10);
                current_field++;
                break;
              case SESS_FIELD_REQ_NUM:
                tmp_session.req_num
                    = (int) strtol(tmp_buff, (char **)NULL, 10);
                current_field++;
                break;
              case SESS_FIELD_ERR_NUM:
                tmp_session.error_num
                    = (int) strtol(tmp_buff, (char **)NULL, 10);
                current_field++;
                break;
              case SESS_FIELD_UPTIME:
                tmp_session.uptime
                    = (time_t) strtol(tmp_buff, (char **)NULL, 10);
                current_field++;
                break;
            }
            
            tmp_buff[0] = '\0';
            tmp_buff_cnt = 0;
        } else if (buffer[i] == '\n') {
            if (strstr(tmp_buff, "EORS") != NULL)
                break;
        
            tmp_buff[tmp_buff_cnt] = '\0';
            strcpy(tmp_session.permissions, tmp_buff);
            tmp_buff[0] = '\0';
            tmp_buff_cnt = 0;
            current_field = SESS_FIELD_ID;
            
            __append_session(sessions, &tmp_session);
        } else {
            tmp_buff[tmp_buff_cnt] = buffer[i];
            tmp_buff_cnt++;
        }  
    }
    
    return sessions;
}

int kclient_is_valid_sess_id(struct running_sessions *sessions, int sid)
{
    int i;
    
    for (i=0; i<sessions->sess_num; i++)
        if (sessions->sessions[i].sess_id == sid)
            return 1;
            
    return 0;
}

/*
 * --------------------
 *    Session perfs
 * --------------------
 */
 
 /* Fields of the session perfs */
enum session_perfs_fields {
    SESS_PERFS_TIMING_PT_NAME,
    SESS_PERFS_MEAN,
    SESS_PERFS_MIN,
    SESS_PERFS_MAX,
    session_perfs_fields_num
};
 
/**
 * __append_timing_pt - Add a timing point to the session perfs
 * @perfs: Session perfs to extend
 * @sess_status: Session status to append 
 */
static void __append_timing_pt(struct session_perfs *perfs, 
                               const struct timing_point *time_pt)
{
    strcpy(perfs->points[perfs->timing_points_num].name, time_pt->name);
    perfs->points[perfs->timing_points_num].mean_duration = time_pt->mean_duration;
    perfs->points[perfs->timing_points_num].min_duration = time_pt->min_duration;
    perfs->points[perfs->timing_points_num].max_duration = time_pt->max_duration;

    perfs->timing_points_num++;
}
 
 /**
 * __get_session_perfs_data - Load running sessions data
 * @kcl: Kclient structure
 * @rcv_buffer: Reception buffer to load
 * @sid: Session ID
 *
 * Returns the number of bytes received on success, -1 if failure
 */
static int __get_session_perfs_data(struct kclient *kcl, 
                                    struct rcv_buff *rcv_buffer, int sid)
{
    int bytes_read;
    char cmd[64];
    
    snprintf(cmd, 64, "1|6|%i|\n", sid);
    
    if (kclient_send_string(kcl, cmd) < 0)
        return -1;
    
    bytes_read = kclient_rcv_esc_seq(kcl, rcv_buffer, "EOSP");
    
    if (bytes_read < 0)
        return -1;
        
/*    printf("bytes_read = %u\n", bytes_read);*/
/*    printf("%s\n", rcv_buffer->buffer);*/
    
    return bytes_read;
}

struct session_perfs* kclient_get_session_perfs(struct kclient *kcl, int sid)
{
    int i, tmp_buff_cnt;
    char tmp_buff[2048];
    int current_field = SESS_PERFS_TIMING_PT_NAME;
    struct timing_point tmp_timing_pt;
    struct session_perfs *perfs;
    
    struct rcv_buff rcv_buffer;
    char *buffer = rcv_buffer.buffer;
    int bytes_read = __get_session_perfs_data(kcl, &rcv_buffer, sid);
    
    if (bytes_read < 0) {
        fprintf(stderr, "Can't get session perfs data\n");
        return NULL;
    }
    
    perfs = malloc(sizeof *perfs);
    
    if (perfs == NULL) {
        fprintf(stderr, "Can't allocate session perfs memory\n");
        return NULL;
    }
    
    perfs->sess_id = sid;
    
    // Parse rcv_buffer
    tmp_buff[0] = '\0';
    tmp_buff_cnt = 0;
    perfs->timing_points_num = 0;
    
    for (i=0; i<bytes_read; i++) {
        if (buffer[i] == ':') {
            tmp_buff[tmp_buff_cnt] = '\0';
            
            switch (current_field) {
              case SESS_PERFS_TIMING_PT_NAME:
                strcpy(tmp_timing_pt.name, tmp_buff);
                current_field++;
                break;
              case SESS_PERFS_MEAN:
                tmp_timing_pt.mean_duration 
                    = (float) strtof(tmp_buff, (char **)NULL);
                current_field++;
                break;
              case SESS_PERFS_MIN:
                tmp_timing_pt.min_duration
                    = (int) strtol(tmp_buff, (char **)NULL, 10);
                current_field++;
                break;
            }
            
            tmp_buff[0] = '\0';
            tmp_buff_cnt = 0;
        } else if (buffer[i] == '\n') {
            if (strstr(tmp_buff, "EOSP") != NULL)
                break;
        
            tmp_buff[tmp_buff_cnt] = '\0';
            tmp_timing_pt.max_duration
                = (int) strtol(tmp_buff, (char **)NULL, 10);
            tmp_buff[0] = '\0';
            tmp_buff_cnt = 0;
            current_field = SESS_PERFS_TIMING_PT_NAME;
            
            __append_timing_pt(perfs, &tmp_timing_pt);
        } else {
            tmp_buff[tmp_buff_cnt] = buffer[i];
            tmp_buff_cnt++;
        }  
    }
    
    return perfs;
}

