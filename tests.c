#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>

#include <mqtt.h>
#include <mqtt_client.h>

int conf_client(const char* addr, const char* port, const struct addrinfo* hints, struct sockaddr_storage* sockaddr) {
    int sockfd = -1;
    int rv;
    struct addrinfo *p, *servinfo;
    char errbuf[128];

    /* get address information */
    rv = getaddrinfo(addr, port, hints, &servinfo);
    if(rv != 0) {
        fprintf(stderr, "error: %s: line %d: getaddrinfo: %s\n",
            __FILE__, __LINE__ - 3, gai_strerror(rv)
        );
        return -1;
    }

    /* open the first possible socket */
    for(p = servinfo; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) {
            sprintf(errbuf, "error: %s: line %d: socket: ", __FILE__, __LINE__ - 2);
            perror(errbuf);
            continue;
        }

        /* connect to server */
        rv = connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen);
        if(rv == -1) {
            sprintf(errbuf, "error: %s: line %d: connect: ", __FILE__, __LINE__ - 2);
            perror(errbuf);
            continue;
        }
        break;
    }  

    /* memcpy the configured socket info */
    if(sockaddr != NULL) memcpy(sockaddr, p->ai_addr, p->ai_addrlen);

    /* free servinfo */
    freeaddrinfo(servinfo);

    /* return the new socket fd */
    return sockfd;  
}

static void test_mqtt_fixed_header(void** state) {
    uint8_t correct_buf[1024];
    uint8_t buf[1024];
    struct mqtt_response response;
    struct mqtt_fixed_header *fixed_header = &response.fixed_header;
    ssize_t rv;

    /* sanity check with valid fixed_header */
    correct_buf[0] = (MQTT_CONTROL_CONNECT << 4) | 0;
    correct_buf[1] = 193u;
    correct_buf[2] = 2u;

    /* check that unpack is correct */
    rv = mqtt_unpack_fixed_header(&response, correct_buf, sizeof(correct_buf));
    assert_true(rv == 3);
    assert_true(fixed_header->control_type == MQTT_CONTROL_CONNECT);
    assert_true(fixed_header->control_flags == 0);
    assert_true(fixed_header->remaining_length == 321);

    /* check that unpack is correct */
    rv = mqtt_pack_fixed_header(buf, sizeof(buf), fixed_header);
    assert_true(rv == 3);
    assert_true(memcmp(correct_buf, buf, 3) == 0);


    /* check that invalid flags are caught */
    correct_buf[0] = (MQTT_CONTROL_CONNECT << 4) | 1;
    rv = mqtt_unpack_fixed_header(&response, correct_buf, sizeof(correct_buf));
    assert_true(rv == MQTT_ERROR_CONTROL_INVALID_FLAGS);

    /* check that valid flags are ok when there is a required bit */
    correct_buf[0] = (MQTT_CONTROL_PUBREL << 4) | 2;
    rv = mqtt_unpack_fixed_header(&response, correct_buf, sizeof(correct_buf));
    assert_true(rv == 3);

    /* check that invalid flags are ok when there is a required bit */
    correct_buf[0] = (MQTT_CONTROL_PUBREL << 4) | 3;
    rv = mqtt_unpack_fixed_header(&response, correct_buf, sizeof(correct_buf));
    assert_true(rv == MQTT_ERROR_CONTROL_INVALID_FLAGS);

    /* check that valid flags are ok when there are optional flags */
    correct_buf[0] = (MQTT_CONTROL_PUBLISH << 4) | 0xF;
    rv = mqtt_unpack_fixed_header(&response, correct_buf, sizeof(correct_buf));
    assert_true(rv == 3);
    
    correct_buf[0] = (MQTT_CONTROL_PUBLISH << 4) | 3;
    rv = mqtt_unpack_fixed_header(&response, correct_buf, sizeof(correct_buf));
    assert_true(rv == 3);


    /* check that remaining length is packed/unpacked correctly */
    correct_buf[0] = (MQTT_CONTROL_CONNECT << 4) | 0;
    correct_buf[1] = 64;

    /* check that unpack is correct */
    rv = mqtt_unpack_fixed_header(&response, correct_buf, sizeof(correct_buf));
    assert_true(rv == 2);
    assert_true(fixed_header->control_type == MQTT_CONTROL_CONNECT);
    assert_true(fixed_header->control_flags == 0);
    assert_true(fixed_header->remaining_length == 64);

    /* check that unpack is correct */
    rv = mqtt_pack_fixed_header(buf, sizeof(buf), fixed_header);
    assert_true(rv == 2);
    assert_true(memcmp(correct_buf, buf, 2) == 0);


    /* check that remaining length is packed/unpacked correctly */
    correct_buf[0] = (MQTT_CONTROL_CONNECT << 4) | 0;
    correct_buf[1] = 127;

    /* check that unpack is correct */
    rv = mqtt_unpack_fixed_header(&response, correct_buf, sizeof(correct_buf));
    assert_true(rv == 2);
    assert_true(fixed_header->control_type == MQTT_CONTROL_CONNECT);
    assert_true(fixed_header->control_flags == 0);
    assert_true(fixed_header->remaining_length == 127);

    /* check that unpack is correct */
    rv = mqtt_pack_fixed_header(buf, sizeof(buf), fixed_header);
    assert_true(rv == 2);
    assert_true(memcmp(correct_buf, buf, 2) == 0);


    /* check that remaining length is packed/unpacked correctly */
    correct_buf[0] = (MQTT_CONTROL_CONNECT << 4) | 0;
    correct_buf[1] = 128;
    correct_buf[2] = 1;

    /* check that unpack is correct */
    rv = mqtt_unpack_fixed_header(&response, correct_buf, sizeof(correct_buf));
    assert_true(rv == 3);
    assert_true(fixed_header->control_type == MQTT_CONTROL_CONNECT);
    assert_true(fixed_header->control_flags == 0);
    assert_true(fixed_header->remaining_length == 128);

    /* check that unpack is correct */
    rv = mqtt_pack_fixed_header(buf, sizeof(buf), fixed_header);
    assert_true(rv == 3);
    assert_true(memcmp(correct_buf, buf, 3) == 0);

    /* check bad inputs */
    assert_true( mqtt_pack_fixed_header(NULL, 5, fixed_header) == MQTT_ERROR_NULLPTR );
    assert_true( mqtt_pack_fixed_header(buf, 5, NULL) == MQTT_ERROR_NULLPTR );
    assert_true( mqtt_pack_fixed_header(buf, 2, fixed_header) == 0 );

    assert_true( mqtt_unpack_fixed_header(NULL, buf, 5) == MQTT_ERROR_NULLPTR );
    assert_true( mqtt_unpack_fixed_header(&response, NULL, 5) == MQTT_ERROR_NULLPTR );
    assert_true( mqtt_unpack_fixed_header(&response, buf, 2) == 0 );
}

static void test_mqtt_pack_connection_request(void** state) {
    uint8_t buf[256];
    ssize_t rv;
    const uint8_t correct_bytes[] = {
        (MQTT_CONTROL_DISCONNECT << 4) | 0, 16,
        0, 4, 'M', 'Q', 'T', 'T', MQTT_PROTOCOL_LEVEL, 0, 120u, 
        0, 4, 'l', 'i', 'a', 'm'
    };
    struct mqtt_response response;
    struct mqtt_fixed_header *fixed_header = &response.fixed_header;

    rv = mqtt_pack_connection_request(buf, sizeof(buf), "liam", NULL, NULL, NULL, NULL, 0, 120u);
    assert_true(rv == 18);

    /* check that fixed header is correct */
    rv = mqtt_unpack_fixed_header(&response, buf, rv);
    assert_true(fixed_header->control_type == MQTT_CONTROL_CONNECT);
    assert_true(fixed_header->remaining_length == 16);

    /* check that memory is correct */
    assert_true(memcmp(correct_bytes, buf, sizeof(correct_bytes)));
}

static void test_mqtt_pack_publish(void** state) {
    uint8_t buf[256];
    ssize_t rv;
    const uint8_t correct_bytes[] = {
        (MQTT_CONTROL_PUBLISH << 4) | MQTT_PUBLISH_RETAIN, 20,
        0, 6, 't', 'o', 'p', 'i', 'c', '1', 0, 23,
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'
    };
    struct mqtt_response mqtt_response;
    struct mqtt_response_publish *response;
    response = &(mqtt_response.decoded.publish);
    
    
    rv = mqtt_pack_publish_request(buf, 256, "topic1", 23, "0123456789", 10, MQTT_PUBLISH_RETAIN);
    assert_true(rv == 22);
    assert_true(memcmp(buf, correct_bytes, 22) == 0);

    rv = mqtt_unpack_fixed_header(&mqtt_response, buf, 22);
    assert_true(rv == 2);
    rv = mqtt_unpack_publish_response(&mqtt_response, buf + 2);
    assert_true(response->qos_level == 0);
    assert_true(response->retain_flag == 1);
    assert_true(response->dup_flag == 0);
    assert_true(response->topic_name_size == 6);
    assert_true(memcmp(response->topic_name, "topic1", 6) == 0);
    assert_true(response->application_message_size == 10);
    assert_true(memcmp(response->application_message, "0123456789", 10) == 0);
}

static void test_mosquitto_connect_disconnect(void** state) {
    uint8_t buf[256];
    const char* addr = "test.mosquitto.org";
    const char* port = "1883";
    struct addrinfo hints = {0};
    struct sockaddr_storage sockaddr;
    struct mqtt_client client;
    ssize_t rv;
    struct mqtt_response mqtt_response;

    hints.ai_family = AF_INET;         /* use IPv4 */
    hints.ai_socktype = SOCK_STREAM;    /* TCP */
    client.socketfd = conf_client(addr, port, &hints, &sockaddr);
    assert_true(client.socketfd != -1);

    rv = mqtt_pack_connection_request(buf, sizeof(buf), "liam-123456", NULL, NULL, NULL, NULL, 0, 30);
    assert_true(rv > 0);
    assert_true(send(client.socketfd, buf, rv, 0) != -1);

    /* receive connack */
    assert_true(recv(client.socketfd, buf, sizeof(buf), 0) != -1);
    rv = mqtt_unpack_fixed_header(&mqtt_response, buf, sizeof(buf));
    assert_true(rv > 0);
    assert_true(mqtt_unpack_connack_response(&mqtt_response, buf + rv) > 0);
    assert_true(mqtt_response.decoded.connack.return_code == MQTT_CONNACK_ACCEPTED);

    /* disconnect */
    rv = mqtt_pack_disconnect(buf, sizeof(buf));
    assert_true(rv > 0);
    assert_true(send(client.socketfd, buf, rv, 0) != -1);

    /*close the socket */
    close(client.socketfd);
}

static void test_mqtt_unpack_connection_response(void** state) {
    uint8_t buf[] = {
        (MQTT_CONTROL_CONNACK << 4) | 0, 2,        
        0, MQTT_CONNACK_ACCEPTED
    };
    struct mqtt_response mqtt_response;
    ssize_t rv = mqtt_unpack_fixed_header(&mqtt_response, buf, sizeof(buf));
    assert_true(rv == 2);
    assert_true(mqtt_response.fixed_header.control_type == MQTT_CONTROL_CONNACK);

    /* unpack response */
    rv = mqtt_unpack_connack_response(&mqtt_response, buf+2);
    assert_true(rv == 2);
    assert_true(mqtt_response.decoded.connack.session_present_flag == 0);
    assert_true(mqtt_response.decoded.connack.return_code == MQTT_CONNACK_ACCEPTED);
}

static void test_mqtt_pubxxx(void** state) {
    uint8_t buf[256];
    ssize_t rv;
    struct mqtt_response response;
    uint8_t puback_correct_bytes[] = {
        MQTT_CONTROL_PUBACK << 4, 2,
        0, 213u
    };
    uint8_t pubrec_correct_bytes[] = {
        MQTT_CONTROL_PUBREC << 4, 2,
        0, 213u
    };
    uint8_t pubrel_correct_bytes[] = {
        MQTT_CONTROL_PUBREL << 4 | 2u, 2,
        0, 213u
    };
    uint8_t pubcomp_correct_bytes[] = {
        MQTT_CONTROL_PUBCOMP << 4, 2,
        0, 213u
    };

    /* puback */
    rv = mqtt_pack_pubxxx_request(buf, 256, MQTT_CONTROL_PUBACK, 213u);
    assert_true(rv == 4);
    assert_true(memcmp(puback_correct_bytes, buf, 4) == 0);

    rv = mqtt_unpack_fixed_header(&response, buf, 256);
    assert_true(rv == 2);
    assert_true(response.fixed_header.control_type == MQTT_CONTROL_PUBACK);
    rv = mqtt_unpack_pubxxx_response(&response, buf + 2);
    assert_true(rv == 2);
    assert_true(response.decoded.puback.packet_id == 213u);

    /* pubrec */
    rv = mqtt_pack_pubxxx_request(buf, 256, MQTT_CONTROL_PUBREC, 213u);
    assert_true(rv == 4);
    assert_true(memcmp(pubrec_correct_bytes, buf, 4) == 0);

    rv = mqtt_unpack_fixed_header(&response, buf, 256);
    assert_true(rv == 2);
    assert_true(response.fixed_header.control_type == MQTT_CONTROL_PUBREC);
    rv = mqtt_unpack_pubxxx_response(&response, buf + 2);
    assert_true(rv == 2);
    assert_true(response.decoded.pubrec.packet_id == 213u);

    /* pubrel */
    rv = mqtt_pack_pubxxx_request(buf, 256, MQTT_CONTROL_PUBREL, 213u);
    assert_true(rv == 4);
    assert_true(memcmp(pubrel_correct_bytes, buf, 4) == 0);

    rv = mqtt_unpack_fixed_header(&response, buf, 256);
    assert_true(rv == 2);
    assert_true(response.fixed_header.control_type == MQTT_CONTROL_PUBREL);
    rv = mqtt_unpack_pubxxx_response(&response, buf + 2);
    assert_true(rv == 2);
    assert_true(response.decoded.pubrel.packet_id == 213u);

    /* pubcomp */
    rv = mqtt_pack_pubxxx_request(buf, 256, MQTT_CONTROL_PUBCOMP, 213u);
    assert_true(rv == 4);
    assert_true(memcmp(pubcomp_correct_bytes, buf, 4) == 0);

    rv = mqtt_unpack_fixed_header(&response, buf, 256);
    assert_true(rv == 2);
    assert_true(response.fixed_header.control_type == MQTT_CONTROL_PUBCOMP);
    rv = mqtt_unpack_pubxxx_response(&response, buf + 2);
    assert_true(rv == 2);
    assert_true(response.decoded.pubcomp.packet_id == 213u);
}

static void test_mqtt_pack_subscribe(void** state) {
    uint8_t buf[256];
    ssize_t rv;
    const uint8_t correct[] = {
        MQTT_CONTROL_SUBSCRIBE << 4 | 2u, 23,
        0, 132u,
        0, 3, 'a', '/', 'b', 0u,
        0, 5, 'b', 'b', 'b', '/', 'x', 1u,
        0, 4, 'c', '/', 'd', 'd', 0u,
    };

    rv = mqtt_pack_subscribe_request(buf, 256, 132, "a/b", 0, "bbb/x", 1, "c/dd", 0, NULL);
    assert_true(rv == 25);
    assert_true(memcmp(buf, correct, 25) == 0);
}

static void test_mqtt_unpack_suback(void** state) {
    ssize_t rv;
    struct mqtt_response response;
    const uint8_t buf[] = {
        MQTT_CONTROL_SUBACK << 4, 5,
        0, 132u,
        MQTT_SUBACK_SUCCESS_MAX_QOS_0,
        MQTT_SUBACK_SUCCESS_MAX_QOS_1,
        MQTT_SUBACK_FAILURE
    };
    rv = mqtt_unpack_fixed_header(&response, buf, sizeof(buf));
    assert_true(rv == 2);
    assert_true(response.fixed_header.control_type == MQTT_CONTROL_SUBACK);
    rv = mqtt_unpack_suback_response(&response, buf + 2);
    assert_true(rv == 5);
    assert_true(response.decoded.suback.packet_id == 132u);
    assert_true(response.decoded.suback.num_return_codes == 3);
    assert_true(response.decoded.suback.return_codes[0] == MQTT_SUBACK_SUCCESS_MAX_QOS_0);
    assert_true(response.decoded.suback.return_codes[1] == MQTT_SUBACK_SUCCESS_MAX_QOS_1);
    assert_true(response.decoded.suback.return_codes[2] == MQTT_SUBACK_FAILURE);
}

static void test_mqtt_pack_unsubscribe(void** state) {
    uint8_t buf[256];
    ssize_t rv;
    const uint8_t correct[] = {
        MQTT_CONTROL_UNSUBSCRIBE << 4 | 2u, 20,
        0, 132u,
        0, 3, 'a', '/', 'b',
        0, 5, 'b', 'b', 'b', '/', 'x',
        0, 4, 'c', '/', 'd', 'd',
    };

    rv = mqtt_pack_unsubscribe_request(buf, 256, 132, "a/b", "bbb/x", "c/dd", NULL);
    assert_true(rv == 22);
    assert_true(memcmp(buf, correct, sizeof(correct)) == 0);
}

static void test_mqtt_unpack_unsuback(void** state) {
    uint8_t buf[] = {
        MQTT_CONTROL_UNSUBACK << 4, 2,
        0, 213u
    };
    ssize_t rv;
    struct mqtt_response response;

    rv = mqtt_unpack_fixed_header(&response, buf, 4);
    assert_true(rv == 2);
    assert_true(response.fixed_header.control_type == MQTT_CONTROL_UNSUBACK);
    rv = mqtt_unpack_unsuback_response(&response, buf + 2);
    assert_true(rv == 2);
    assert_true(response.decoded.unsuback.packet_id == 213u);
}

static void test_mqtt_pack_disconnect(void** state) {
    uint8_t buf[2];
    assert_true(mqtt_pack_disconnect(buf, 2) == 2);   
}

static void test_mqtt_pack_ping(void** state) {
    uint8_t buf[2];
    struct mqtt_response response;
    struct mqtt_fixed_header *fixed_header = &response.fixed_header;
    assert_true(mqtt_pack_ping_request(buf, 2) == 2);   
    assert_true(mqtt_unpack_fixed_header(&response, buf, 2) == 2);
    assert_true(fixed_header->control_type == MQTT_CONTROL_PINGREQ);
    assert_true(fixed_header->remaining_length == 0);
}

static void test_mqtt_connect_and_ping(void** state) {
    uint8_t buf[256];
    const char* addr = "test.mosquitto.org";
    const char* port = "1883";
    struct addrinfo hints = {0};
    struct sockaddr_storage sockaddr;
    struct mqtt_client client;
    ssize_t rv;
    struct mqtt_response mqtt_response;

    hints.ai_family = AF_INET;         /* use IPv4 */
    hints.ai_socktype = SOCK_STREAM;    /* TCP */
    client.socketfd = conf_client(addr, port, &hints, &sockaddr);
    assert_true(client.socketfd != -1);

    rv = mqtt_pack_connection_request(buf, sizeof(buf), "this-is-me", NULL, NULL, NULL, NULL, 0, 30);
    assert_true(rv > 0);
    assert_true(send(client.socketfd, buf, rv, 0) != -1);

    /* receive connack */
    assert_true(recv(client.socketfd, buf, sizeof(buf), 0) != -1);
    rv = mqtt_unpack_fixed_header(&mqtt_response, buf, sizeof(buf));
    assert_true(rv > 0);
    assert_true(mqtt_unpack_connack_response(&mqtt_response, buf + rv) > 0);
    assert_true(mqtt_response.decoded.connack.return_code == MQTT_CONNACK_ACCEPTED);

    /* send ping request */
    rv = mqtt_pack_ping_request(buf, sizeof(buf));
    assert_true(rv > 0);
    assert_true(send(client.socketfd, buf, rv, 0) != -1);

    /* receive ping response */
    assert_true(recv(client.socketfd, buf, sizeof(buf), 0) != -1);
    rv = mqtt_unpack_fixed_header(&mqtt_response, buf, sizeof(buf));
    assert_true(rv > 0);
    assert_true(mqtt_response.fixed_header.control_type == MQTT_CONTROL_PINGRESP);

    /* disconnect */
    rv = mqtt_pack_disconnect(buf, sizeof(buf));
    assert_true(rv > 0);
    assert_true(send(client.socketfd, buf, rv, 0) != -1);

    /*close the socket */
    close(client.socketfd);
}

#define QM_SZ (int) sizeof(struct mqtt_queued_message)
static void test_message_queue(void **unused) {
    uint8_t mem[32 + 4*QM_SZ];
    struct mqtt_message_queue mq;
    struct mqtt_queued_message *tail;
    mqtt_mq_init(&mq, mem, sizeof(mem));

    /* check that it fills up correctly */
    assert_true(mqtt_mq_length(&mq) == 0);
    assert_true(mq.curr_sz == 32 + 3*QM_SZ);
    memset(mq.curr, 0, 8);
    tail = mqtt_mq_register(&mq, 8);
    tail->control_type = 2;
    tail->packet_id = 111;
    assert_true(mqtt_mq_length(&mq) == 1);
    assert_true(mq.curr_sz == 24 + 2*QM_SZ);
    memset(mq.curr, 1, 8);
    tail = mqtt_mq_register(&mq, 8);
    tail->control_type = 3;
    tail->packet_id = 222;
    assert_true(mqtt_mq_length(&mq) == 2);
    assert_true(mq.curr_sz == 16 + 1*QM_SZ);
    memset(mq.curr, 2, 8);
    tail = mqtt_mq_register(&mq, 8);
    tail->control_type = 4;
    tail->packet_id = 333;
    assert_true(mqtt_mq_length(&mq) == 3);
    assert_true(mq.curr_sz == 8);
    memset(mq.curr, 3, 8);
    tail = mqtt_mq_register(&mq, 8);
    tail->control_type = 5;
    tail->packet_id = 444;
    assert_true(mqtt_mq_length(&mq) == 4);
    assert_true(mq.curr_sz == 0);
    assert_true(mq.curr == (uint8_t*) mq.queue_tail);

    /* check that start's are correct */
    for(unsigned int i = 0; i < 4; ++i) {
        assert_true(mqtt_mq_get(&mq, i)->start == (uint8_t*) mq.mem_start + 8*i);
        for(int j = 0; j < 8; ++j) {
            assert_true(mqtt_mq_get(&mq, i)->start[j] == i);
        }

        assert_true(mqtt_mq_get(&mq, i)->control_type == i + 2);
        assert_true(mqtt_mq_get(&mq, i)->packet_id == 111 * (i + 1));
    }

    /* check that it cleans correctly */
    mqtt_mq_clean(&mq);   /* should do nothing */
    assert_true(mqtt_mq_length(&mq) == 4);
    assert_true(mq.curr_sz == 0);
    assert_true(mq.curr == (uint8_t*) mq.queue_tail);

    /* try clearing middle (should do nothing) */
    mqtt_mq_get(&mq, 1)->state = MQTT_QUEUED_COMPLETE;
    mqtt_mq_get(&mq, 0)->state = MQTT_QUEUED_AWAITING_ACK;
    mqtt_mq_clean(&mq);
    assert_true(mqtt_mq_length(&mq) == 4);
    assert_true(mq.curr_sz == 0);
    assert_true(mq.curr == (uint8_t*) mq.queue_tail);

    /* complete first then clean (should clear 2) */
    mqtt_mq_get(&mq, 0)->state = MQTT_QUEUED_COMPLETE;
    mqtt_mq_clean(&mq);
    assert_true(mqtt_mq_length(&mq) == 2);
    assert_true(mq.curr_sz == 16 + 1*QM_SZ);
    assert_true(mq.curr == mem + 16);

    /* check that start's are correct */
    for(unsigned int i = 0; i < 2; ++i) {
        assert_true(mqtt_mq_get(&mq, i)->start == (uint8_t*) mq.mem_start + 8*i);
        for(int j = 0; j < 8; ++j) {
            assert_true(mqtt_mq_get(&mq, i)->start[j] == i+2); /* check value */
        }
        assert_true(mqtt_mq_get(&mq, i)->control_type == i + 4);
        assert_true(mqtt_mq_get(&mq, i)->packet_id == 111 * (i + 3));
    }

    /* remove the last two */
    mqtt_mq_get(&mq, 0)->state = MQTT_QUEUED_COMPLETE;
    mqtt_mq_get(&mq, 1)->state = MQTT_QUEUED_COMPLETE;
    mqtt_mq_clean(&mq); 
    assert_true(mqtt_mq_length(&mq) == 0);
    assert_true(mq.curr_sz == 32 + 3*QM_SZ);
    assert_true((void*) mq.queue_tail == mq.mem_end);
}

static void test_packet_id_lfsr(void **unused) {
    struct mqtt_client client;
    client.pid_lfsr = 163u;
    uint32_t period = 0;
    do {
        __mqtt_next_pid(&client);
        period++;
    } while(client.pid_lfsr != 163u && client.pid_lfsr !=0);
    assert_true(period == 65535u);
}

void publish_callback(void** state, struct mqtt_response_publish *publish) {
    /*char *name = (char*) malloc(publish->topic_name_size + 1);
    memcpy(name, publish->topic_name, publish->topic_name_size);
    name[publish->topic_name_size] = '\0';
    printf("Received a PUBLISH(topic=%s, DUP=%d, QOS=%d, RETAIN=%d, pid=%d) from the broker. Data='%s'\n", 
           name, publish->dup_flag, publish->qos_level, publish->retain_flag, publish->packet_id,
           (const char*) (publish->application_message)
    );
    free(name);*/
    **(int**)state += 1;
}

static void test_client_simple(void **unused) {
    uint8_t sendmem[2048];
    uint8_t recvmem[1024];
    const char* addr = "test.mosquitto.org";
    const char* port = "1883";
    struct addrinfo hints = {0};
    struct mqtt_client client;
    ssize_t rv;

    hints.ai_family = AF_INET;          /* use IPv4 */
    hints.ai_socktype = SOCK_STREAM;    /* TCP */
    int sockfd = conf_client(addr, port, &hints, NULL);
    fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);

    /* initialize */
    mqtt_init(&client, sockfd, sendmem, sizeof(sendmem), recvmem, sizeof(recvmem), publish_callback);

    /* connect */
    assert_true(mqtt_connect(&client, "liam-123", NULL, NULL, NULL, NULL, 0, 30) > 0);
    assert_true(__mqtt_send(&client) > 0);
    while(mqtt_mq_length(&client.mq) > 0) {
        assert_true(__mqtt_recv(&client) > 0);
        mqtt_mq_clean(&client.mq);
        usleep(10000);
    }

    /* ping */
    assert_true(mqtt_ping(&client) > 0);
    while(mqtt_mq_length(&client.mq) > 0) {
        rv = __mqtt_send(&client);
        if (rv <= 0) {
            printf("error: %s\n", mqtt_error_str(rv));
            assert_true(0);
        }
        rv = __mqtt_recv(&client);
        if (rv <= 0) {
            printf("error: %s\n", mqtt_error_str(rv));
            assert_true(0);
        }
        mqtt_mq_clean(&client.mq);
        usleep(10000);
    }

    /* disconnect */
    assert_true(client.error == MQTT_OK);
    assert_true(mqtt_disconnect(&client) > 0);
    assert_true(__mqtt_send(&client) > 0);
}

static void test_client_simple_subpub(void **unused) {
    uint8_t sendmem1[2048], sendmem2[2048];
    uint8_t recvmem1[1024], recvmem2[1024];
    const char* addr = "test.mosquitto.org";
    const char* port = "1883";
    struct addrinfo hints;
    struct mqtt_client sender, receiver;

    int state = 0;

    /* initialize sender */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;          /* use IPv4 */
    hints.ai_socktype = SOCK_STREAM;    /* TCP */
    int sockfd = conf_client(addr, port, &hints, NULL);
    fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);
    mqtt_init(&sender, sockfd, sendmem1, sizeof(sendmem1), recvmem1, sizeof(recvmem1), publish_callback);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;          /* use IPv4 */
    hints.ai_socktype = SOCK_STREAM;    /* TCP */
    sockfd = conf_client(addr, port, &hints, NULL);
    fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);
    mqtt_init(&receiver, sockfd, sendmem2, sizeof(sendmem2), recvmem2, sizeof(recvmem2), publish_callback);
    receiver.publish_response_callback_state = &state;

    /* connect both */
    assert_true(mqtt_connect(&sender, "liam-123", NULL, NULL, NULL, NULL, 0, 30) > 0);
    assert_true(mqtt_connect(&receiver, "liam-234", NULL, NULL, NULL, NULL, 0, 30) > 0);
    assert_true(__mqtt_send(&sender) > 0);
    assert_true(__mqtt_send(&receiver) > 0);
    while(mqtt_mq_length(&sender.mq) > 0 || mqtt_mq_length(&receiver.mq) > 0) {
        assert_true(__mqtt_recv(&sender) > 0);
        mqtt_mq_clean(&sender.mq);
        assert_true(__mqtt_recv(&receiver) > 0);
        mqtt_mq_clean(&receiver.mq);
        usleep(10000);
    }

    /* subscribe receiver*/
    assert_true(mqtt_subscribe(&receiver, "liam-test-topic", 2) > 0);
    assert_true(__mqtt_send(&receiver) > 0);
    while(mqtt_mq_length(&receiver.mq) > 0) {
        assert_true(__mqtt_recv(&receiver) > 0);
        mqtt_mq_clean(&receiver.mq);
        usleep(10000);
    }

    /* publish from sender */
    assert_true(mqtt_publish(&sender, "liam-test-topic", "data", 5, MQTT_PUBLISH_QOS_0) > 0);
    assert_true(__mqtt_send(&sender) > 0);

    time_t start = time(NULL);
    while(state == 0 && time(NULL) < start + 10) {
        assert_true(__mqtt_recv(&receiver) > 0);        
        usleep(10000);
    }

    assert_true(state == 1);

    /* disconnect */
    assert_true(sender.error == MQTT_OK);
    assert_true(receiver.error == MQTT_OK);
    assert_true(mqtt_disconnect(&sender) > 0);
    assert_true(mqtt_disconnect(&receiver) > 0);
    assert_true(__mqtt_send(&sender) > 0);
    assert_true(__mqtt_send(&receiver) > 0);
}


#define TEST_PACKET_SIZE (149)
#define TEST_DATA_SIZE (128)
static void test_client_subpub(void **unused) {
    uint8_t sendmem1[TEST_PACKET_SIZE*4 + sizeof(struct mqtt_queued_message)*4], 
            sendmem2[TEST_PACKET_SIZE*4 + sizeof(struct mqtt_queued_message)*4];
    uint8_t recvmem1[TEST_PACKET_SIZE], recvmem2[TEST_PACKET_SIZE];
    const char* addr = "test.mosquitto.org";
    const char* port = "1883";
    struct addrinfo hints;
    struct mqtt_client sender, receiver;
    ssize_t rv;

    int state = 0;

    /* initialize sender */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;          /* use IPv4 */
    hints.ai_socktype = SOCK_STREAM;    /* TCP */
    int sockfd = conf_client(addr, port, &hints, NULL);
    fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);
    mqtt_init(&sender, sockfd, sendmem1, sizeof(sendmem1), recvmem1, sizeof(recvmem1), publish_callback);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;          /* use IPv4 */
    hints.ai_socktype = SOCK_STREAM;    /* TCP */
    sockfd = conf_client(addr, port, &hints, NULL);
    fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);
    mqtt_init(&receiver, sockfd, sendmem2, sizeof(sendmem2), recvmem2, sizeof(recvmem2), publish_callback);
    receiver.publish_response_callback_state = &state;

    /* connect both */
    if ((rv = mqtt_connect(&sender, "liam-123", NULL, NULL, NULL, NULL, MQTT_CONNECT_CLEAN_SESSION, 30)) <= 0) {
        printf("error: %s\n", mqtt_error_str(rv));
        assert_true(rv  > 0);
    }
    if ((rv = mqtt_connect(&receiver, "liam-234", NULL, NULL, NULL, NULL, MQTT_CONNECT_CLEAN_SESSION, 30)) <= 0) {
        printf("error: %s\n", mqtt_error_str(rv));
        assert_true(rv  > 0);
    }
    if ((rv = __mqtt_send(&sender)) <= 0) {
        printf("error: %s\n", mqtt_error_str(rv));
        assert_true(rv  > 0);
    }
    if ((rv = __mqtt_send(&receiver)) <= 0) {
        printf("error: %s\n", mqtt_error_str(rv));
        assert_true(rv  > 0);
    }
    while(mqtt_mq_length(&sender.mq) > 0 || mqtt_mq_length(&receiver.mq) > 0) {
        if ((rv = __mqtt_recv(&sender)) <= 0) {
            printf("error: %s\n", mqtt_error_str(rv));
            assert_true(rv  > 0);
        }
        mqtt_mq_clean(&sender.mq);
        if ((rv = __mqtt_recv(&receiver)) <= 0) {
            printf("error: %s\n", mqtt_error_str(rv));
            assert_true(rv  > 0);
        }
        mqtt_mq_clean(&receiver.mq);
        usleep(10000);
    }

    state = 0;

    /* publish with retain */
    if ((rv = mqtt_publish(&sender, "liam-test-ret1", "this was initial retain with qos 1", TEST_DATA_SIZE, MQTT_PUBLISH_QOS_1 | MQTT_PUBLISH_RETAIN)) <= 0) {
        printf("error: %s\n", mqtt_error_str(rv));
        assert_true(rv  > 0);
    }
    if ((rv = __mqtt_send(&sender)) <= 0) {
        printf("error: %s\n", mqtt_error_str(rv));
        assert_true(rv  > 0);
    }

    /* subscribe receiver*/
    if ((rv = mqtt_subscribe(&receiver, "liam-test-qos0", 0)) <= 0) {
        printf("error: %s\n", mqtt_error_str(rv));
        assert_true(rv  > 0);
    }
    if ((rv = mqtt_subscribe(&receiver, "liam-test-qos1", 1)) <= 0) {
        printf("error: %s\n", mqtt_error_str(rv));
        assert_true(rv  > 0);
    }
    if ((rv = mqtt_subscribe(&receiver, "liam-test-qos2", 2)) <= 0) {
        printf("error: %s\n", mqtt_error_str(rv));
        assert_true(rv  > 0);
    }
    if ((rv = mqtt_subscribe(&receiver, "liam-test-ret1", 2)) <= 0) {
        printf("error: %s\n", mqtt_error_str(rv));
        assert_true(rv  > 0);
    }
    if ((rv = __mqtt_send(&receiver)) <= 0) {
        printf("error: %s\n", mqtt_error_str(rv));
        assert_true(rv  > 0);
    }

    /* wait for retained publish and receiver and sender have 0 length mq's */
    time_t start = time(NULL);
    while(start + 10 > time(NULL)  && (state < 1 || mqtt_mq_length(&receiver.mq) > 0 || mqtt_mq_length(&sender.mq) > 0)) {
        if ((rv = __mqtt_recv(&receiver)) < 0) {
            printf("error: %s\n", mqtt_error_str(rv));
            assert_true(0);
        }
        mqtt_mq_clean(&receiver.mq);
        if ((rv = __mqtt_recv(&sender)) < 0) {
            printf("error: %s\n", mqtt_error_str(rv));
            assert_true(0);
        }
        mqtt_mq_clean(&sender.mq);
        usleep(10000);
    }

    /* make sure that we publish was called */
    assert_true(mqtt_mq_length(&receiver.mq) == 0);
    assert_true(mqtt_mq_length(&sender.mq) == 0);
    assert_true(state == 1);

    /* now publish 4 perfect sized messages */
    if ((rv = mqtt_publish(&sender, "liam-test-ret1", "retain with qos1", TEST_DATA_SIZE, MQTT_PUBLISH_QOS_1)) <= 0) {
        printf("error: %s\n", mqtt_error_str(rv));
        assert_true(rv  > 0);
    }
    if ((rv = mqtt_publish(&sender, "liam-test-qos0", "test with qos 0", TEST_DATA_SIZE, MQTT_PUBLISH_QOS_0)) <= 0) {
        printf("error: %s\n", mqtt_error_str(rv));
        assert_true(rv  > 0);
    }
    if ((rv = mqtt_publish(&sender, "liam-test-qos1", "test with qos 1", TEST_DATA_SIZE, MQTT_PUBLISH_QOS_1)) <= 0) {
        printf("error: %s\n", mqtt_error_str(rv));
        assert_true(rv  > 0);
    }
    if ((rv = mqtt_publish(&sender, "liam-test-qos2", "test with qos 2", TEST_DATA_SIZE, MQTT_PUBLISH_QOS_2)) <= 0) {
        printf("error: %s\n", mqtt_error_str(rv));
        assert_true(rv  > 0);
    }
    if ((rv = __mqtt_send(&sender)) <= 0) {
        printf("error: %s\n", mqtt_error_str(rv));
        assert_true(rv  > 0);
    }
    assert_true(sender.error == MQTT_OK);
    assert_true(sender.mq.curr_sz == 0);

    /* give 2 seconds for sending and receiving (also don't manually clean) */
    start = time(NULL);
    while(time(NULL) < start + 8) {
        if ((rv = __mqtt_recv(&receiver)) < 0) {
            printf("error: %s\n", mqtt_error_str(rv));
            assert_true(0);
        }
        if ((rv = __mqtt_recv(&sender)) < 0) {
            printf("error: %s\n", mqtt_error_str(rv));
            assert_true(0);
        }
        if ((rv = __mqtt_send(&receiver)) < 0) {
            printf("error: %s\n", mqtt_error_str(rv));
            assert_true(0);
        }
        if ((rv = __mqtt_send(&sender)) < 0) {
            printf("error: %s\n", mqtt_error_str(rv));
            assert_true(0);
        }
        usleep(10000);
    }

    if (state != 5) {
        printf("error: state == %d\n", state);
        assert_true(state == 5);
    }

    /* test unsubscribe */
    if ((rv = mqtt_unsubscribe(&receiver, "liam-test-qos1")) <= 0) {
        printf("error: %s\n", mqtt_error_str(rv));
        assert_true(rv  > 0);
    }

    /*sleep for 2 seconds while unsubscribe is sending */ 
    start = time(NULL);
    while(time(NULL) < start + 2) {
        if ((rv = __mqtt_recv(&receiver)) < 0) {
            printf("error: %s\n", mqtt_error_str(rv));
            assert_true(0);
        }
        if ((rv = __mqtt_recv(&sender)) < 0) {
            printf("error: %s\n", mqtt_error_str(rv));
            assert_true(0);
        }
        if ((rv = __mqtt_send(&receiver)) < 0) {
            printf("error: %s\n", mqtt_error_str(rv));
            assert_true(0);
        }
        if ((rv = __mqtt_send(&sender)) < 0) {
            printf("error: %s\n", mqtt_error_str(rv));
            assert_true(0);
        }
        usleep(10000);
    }
    /* publish qos1 (should be received by receiver) */
    if ((rv = mqtt_publish(&sender, "liam-test-qos1", "test with qos 1", TEST_DATA_SIZE, MQTT_PUBLISH_QOS_1)) <= 0) {
        printf("error: %s\n", mqtt_error_str(rv));
        assert_true(rv  > 0);
    }
    /*sleep for 2 seconds to give the publish a chance  */ 
    start = time(NULL);
    while(time(NULL) < start + 2) {
        if ((rv = __mqtt_recv(&receiver)) < 0) {
            printf("error: %s\n", mqtt_error_str(rv));
            assert_true(0);
        }
        if ((rv = __mqtt_recv(&sender)) < 0) {
            printf("error: %s\n", mqtt_error_str(rv));
            assert_true(0);
        }
        if ((rv = __mqtt_send(&receiver)) < 0) {
            printf("error: %s\n", mqtt_error_str(rv));
            assert_true(0);
        }
        if ((rv = __mqtt_send(&sender)) < 0) {
            printf("error: %s\n", mqtt_error_str(rv));
            assert_true(0);
        }
        usleep(10000);
    }

    /* check that the callback wasn't called */
    if (state != 5) {
        printf("error: state == %d\n", state);
        assert_true(state == 5);
    }

    /* disconnect */
    assert_true(sender.error == MQTT_OK);
    assert_true(receiver.error == MQTT_OK);
    if ((rv = mqtt_disconnect(&sender)) <= 0) {
        printf("error: %s\n", mqtt_error_str(rv));
        assert_true(rv  > 0);
    }
    if ((rv = mqtt_disconnect(&receiver)) <= 0) {
        printf("error: %s\n", mqtt_error_str(rv));
        assert_true(rv  > 0);
    }
    if ((rv = __mqtt_send(&sender)) <= 0) {
        printf("error: %s\n", mqtt_error_str(rv));
        assert_true(rv  > 0);
    }
    if ((rv = __mqtt_send(&receiver)) <= 0) {
        printf("error: %s\n", mqtt_error_str(rv));
        assert_true(rv  > 0);
    }
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_mqtt_fixed_header),
        cmocka_unit_test(test_mqtt_pack_connection_request),
        cmocka_unit_test(test_mqtt_unpack_connection_response),
        cmocka_unit_test(test_mqtt_pack_disconnect),
        cmocka_unit_test(test_mosquitto_connect_disconnect),
        cmocka_unit_test(test_mqtt_pack_publish),
        cmocka_unit_test(test_mqtt_pubxxx),
        cmocka_unit_test(test_mqtt_pack_subscribe),
        cmocka_unit_test(test_mqtt_unpack_suback),
        cmocka_unit_test(test_mqtt_pack_unsubscribe),
        cmocka_unit_test(test_mqtt_unpack_unsuback),
        cmocka_unit_test(test_mqtt_pack_ping),
        cmocka_unit_test(test_mqtt_connect_and_ping),
        cmocka_unit_test(test_message_queue),
        cmocka_unit_test(test_packet_id_lfsr),
        cmocka_unit_test(test_client_simple),
        cmocka_unit_test(test_client_simple_subpub),
        cmocka_unit_test(test_client_subpub)
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}