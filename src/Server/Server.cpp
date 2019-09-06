// Server.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Server.h"
#include "afxsock.h"
#include <iostream>
#include <string>
#include <thread>
#include <fstream>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define ServerPort 8888
#define BlackListFileName "blacklist.conf"

using namespace std;

#define CacheDataSize 50000

#pragma region Read Cache file

#define sz_RequestGet	100
#define sz_HostName		100
#define sz_ETag			100
#define sz_CacheData	50000

int CacheUsing = 0;

class Cache
{
private:
	char RequestGet[sz_RequestGet];
	char HostName[sz_HostName];
	char ETag[sz_ETag];
	//byte Data[sz_CacheData];

public:
	Cache()
	{
		memset(RequestGet, '\0', sz_RequestGet);
		memset(HostName, '\0', sz_HostName);
		memset(ETag, '\0', sz_ETag);
		//byte Data[sz_CacheData] = { 0 };
	}

	Cache(char* request, char* host, char* etag)
	{
		memset(RequestGet, '\0', sz_RequestGet);
		memset(HostName, '\0', sz_HostName);
		memset(ETag, '\0', sz_ETag);
		strcpy(RequestGet, request);
		strcpy(HostName, host);
		strcpy(ETag, etag);
	}

	void SaveCache(ofstream& FCout, CSocket& client, CSocket& server)
	{
		FCout << this->RequestGet << '\n';
		FCout << this->HostName << '\n';
		FCout << this->ETag << '\n';
		int i = 0;
		byte* buffer = new byte;
		while (server.Receive(buffer, 1, 0) != 0)
		{
			client.Send(buffer, 1, 0);
			i++;
			FCout << *buffer;
		}
		// thong bao ket thuc Data
		FCout << '\n';
		FCout << '\n';
		FCout << '\n';
		FCout << '\n';
		for (i += 4; i < sz_CacheData; i++)
		{
			FCout << 0;
		}
		delete buffer;
	}


	int CachingProcess(fstream& FCin, CSocket& client, CSocket& server)
	{
		//read line
		char line[sz_CacheData + 1];	//plus one for the last char ('\0')
		FCin.getline(line, sz_RequestGet + 1);	//đọc dòng đầu tiên vào line để bắt đầu tìm

		while (!FCin.eof())
		{
			if (strstr(line, RequestGet) == NULL)	//nếu không tìm được RequestGet
			{
#pragma region dịch 1 đoạn bằng 1 block cache
				FCin.seekg(sz_RequestGet + sz_HostName + sz_ETag + sz_CacheData, ios::cur);	//dịch 1 đoạn bằng độ dài của 1 "khối" cache
				FCin.getline(line, sz_RequestGet + 1);	//đọc tiếp dòng chứa RequestGet cho lần xét sau
#pragma endregion

			}
			else//nếu tìm được RequestGet
			{
#pragma region Xuất khối cache ra console để kiểm tra
				////FCin.getline(line, sz_RequestGet + 1); (đã được đọc -> hiện tại line đang chứa request)
				//cout << line << endl;

				//FCin.getline(line, sz_HostName + 1);
				//cout << line << endl;

				//FCin.getline(line, sz_ETag + 1);
				//cout << line << endl;

				////char data[sz_CacheData + 1];
				//FCin.getline(line, sz_CacheData + 1);
				//cout << line << endl;
				//break;
#pragma endregion

#pragma region Kiểm tra tiếp hostname và etag
//xét host name
				FCin.getline(line, sz_HostName + 1);//line đã chứa sẵn request, vậy getline thêm 1 lần nữa là được dòng chứa host name
				if (strstr(line, HostName) == NULL)	//nếu đây không phải host name đúng
				{
					//FCin.getline(line, sz_ETag + 1);	//đọc tiếp dòng chứa etag
					//FCin.getline(line, sz_CacheData + 1);	//đọc tiếp dòng chứa etag
					FCin.seekg(sz_ETag + sz_CacheData, ios::cur);//quay về vị trí đầu khối cache
					FCin.getline(line, sz_RequestGet + 1);	//đọc tiếp dòng chứa RequestGet cho lần xét sau
				}
				else//tìm được
				{
					//xét ETag
					//nếu ETag giống => trả lại cache
					FCin.getline(line, sz_ETag + 1);//-lúc này line đang chứa Host name, getline lần nữa sẽ được ETag

					if (strstr(line, ETag) != NULL)//nếu tìm thấy etag
					{
						/*char data[sz_CacheData + 1];
						FCin.getline(data, sz_CacheData + 1);*/

						byte* buffer = new byte;
						cout << "Found cache\n";
						for (int i = 0; i < sz_CacheData; i++)
						{
							FCin.read((char*)buffer, 1);
							if (*buffer == '\n')
							{
								byte temp[4];
								int j;
								for (j = 0; i < 4; i++)
								{
									temp[j] = *buffer;
									FCin.read((char*)buffer, 1);
									if (*buffer != '\n')
									{
										client.Send((byte*)temp, j+ 1, 0);
										break;
									}
								}
								if (j == 4)
								{
									delete buffer;
									return 1;
								}
							}
							else
							{
								client.Send(buffer, 1, 0);
							}
							cout << "Sending cache";
						}
						delete buffer;
						return 1;
					}
					else//nếu Etag không giống
					{
						break;//thoát khỏi vòng lặp while -> không tìm thấy
					}

					//(nếu ETag không giống => dữ liệu không còn như trước => load lại và lưu cache mới vào (lưu etag và data) => viết hàm lưu cache)


					break;//thoát khỏi vòng lặp while
				}

#pragma endregion

			}
		}

		//nếu tới đây thì => không tìm thấy cache
		cout << "Cache does not exists!\n";
		return 0;
	}
};


/*----------------------------------------------HAM XU LY REQUEST----------------------------------------------*/

char* get_method(char* request)
{
	char* result = nullptr;
	int n = strlen(request);
	int resultlen = 0;
	for (int i = 0; request[i] != '\r'; i++)
	{
		resultlen++;
	}
	result = new char[resultlen + 1];
	for (int i = 0; request[i] != '\r'; i++)
	{
		result[i] = request[i];
	}
	result[resultlen] = '\0';
	return result;
}

//Get address information from domain name
sockaddr_in* get_addr(char *host)
{
	hostent* HostInfo;
	char* HostIP;
	sockaddr_in* result = new sockaddr_in;
	HostInfo = gethostbyname(host);
	if (HostInfo == nullptr)
	{
		return nullptr;
	}
	HostIP = inet_ntoa(*(struct in_addr *)*HostInfo->h_addr_list); //lay thong tin tu dia chi IP

	// Set up the sockaddr structure
	result->sin_family = AF_INET;
	result->sin_addr.s_addr = inet_addr(HostIP);
	result->sin_port = htons(80);
	return result;

}

// get HOST from request
char* get_host(char* request)
{
	int n = strlen(request);
	char* result = nullptr;
	if (n > 0)
	{
		for (int i = 0; i < n - 6; i++)
		{
			char buffer[7] = { '\0' }; // chua 7 ki tu de so sanh
			strncat_s(buffer, request + i, 6); // lay 6 ki tu bat dau tu con tro i
			if (strcmp(buffer, "Host: ") == 0)
			{
				int resultlen = 0; // dem do dai cua Host
				for (int j = i + 6; request[j] != '\r'; j++) //i+=7: nhay qua "\nHOST: "(7 ki tu)
				{
					resultlen++;
				}
				result = new char[resultlen + 1];
				for (int k = 0, j = i + 6; request[j] != '\r'; j++, k++) //i+=7: nhay qua "\nHOST: "(7 ki tu)
				{
					result[k] = request[j];
				}
				result[resultlen] = '\0';
				break;
			}
		}
	}
	return result;
}

//Get ETag
char* get_ETag(char* request)
{
	int n = strlen(request);
	char* result = nullptr;
	if (n > 0)
	{
		for (int i = 0; i < n - 6; i++)
		{
			char buffer[7] = { '\0' };
			strncat_s(buffer, request + i, 6);
			if (strcmpi(buffer, "ETag: ") == 0) // tim duoc ETag
			{
				int EtagLen = 0;
				for (int j = i + 7; request[j] != '"'; j++)
				{
					EtagLen++;
				}
				result = new char[EtagLen + 1];
				for (int j = i + 7, k = 0; request[j] != '"';j++, k++)
				{
					result[k] = request[j];
				}
				result[EtagLen] = '\0';
				return result;
			}
		}
	}
	return result;
}

// get Connection from request
char* get_connection(char* request)
{
	int n = strlen(request);
	char* result = nullptr;
	if (n > 0)
	{
		for (int i = 0; i < n - 12; i++)
		{
			char buffer[13] = { '\0' };
			strncat_s(buffer, request + i, 12);
			if (strcmp(buffer, "Connection: ") == 0)
			{
				int resultlen = 0;
				for (int j = i + 12; request[j] != '\r'; j++)
				{
					resultlen++;
				}
				result = new char[resultlen + 1];
				for (int k = 0, j = i + 12; request[j] != '\r'; j++, k++)
				{
					result[k] = request[j];
				}
				result[resultlen] = 0;
				break;
			}
		}
	}
	return result;
}


/*----------------------------------------------HAM XU LY BLACKLIST----------------------------------------------*/
bool isInBackList(char* chDomainWeb)
{
	ifstream Fin;
	Fin.open(BlackListFileName, ios::in);
	if (Fin.fail())
	{
		cout << "Failed to open file!" << endl;
		return false;
	}
	//else
	//	cout << "Succeeded to open file!" << endl;


	char* temp = new char[100];
	while (!Fin.eof())
	{
		Fin >> temp;

		if (_strcmpi(temp, chDomainWeb) == 0)//0:= ; <0: str1<str2 ; >0: str1>str2
			return true;
	}
	delete[]temp;

	Fin.close();
	return false;
}

//response
#include<time.h>

string daytime_()
{
	time_t rawtime;
	tm timeinfo;
	char buffer[100];

	time(&rawtime);
	localtime_s(&timeinfo, &rawtime);

	//www.cplusplus.com/reference/ctime/strftime/?kw=strftime
	strftime(buffer, 100, "%a, %d %b %Y %H:%M:%S GMT", &timeinfo);
	string time(buffer);

	return time;
}

char* getResponse403()
{
	std::string res = "";

	string statusline = "HTTP/1.1 403 Fobidden\r\n";
	string time = "Date: " + daytime_() + "\r\n";
	string connection = "Connection: Closed\r\n";
	string type = "Content-Type: text/html\r\n";
	string html = "<html><body><h1>403 - Fobidden: blocked by proxy";
	res = statusline + time + connection + type + '\n' + html + "\r\n";

	char *s = new char[res.length() + 1];
	for (int i = 0; i < res.length(); i++)
		s[i] = res[i];
	s[res.length()] = '\0';

	return s;
}



/*----------------------------------------------HAM KET NOI VOI SERVER----------------------------------------------*/
DWORD WINAPI StartConnection(LPVOID clientSocket)
{
	SOCKET* hConnected = (SOCKET*)clientSocket;
	CSocket client;
	//Chuyen ve lai CSocket
	client.Attach(*hConnected);
	byte* ResponeBuffer = new byte;
	byte*RequestBuffer = new byte;
	byte* request = new byte[1000]; // chua yeu cau duoc gui boi application
	memset(request, '\0', 1000);

	
	if (client.Receive(request, 1000, 0) == SOCKET_ERROR)
	{
		cout << "Can't recieve initial request\n";
		delete hConnected;
		delete[]request;
		delete ResponeBuffer;
		delete RequestBuffer;
		return 0;
	}

	//xu ly request
	char* HostName = get_host((char*)request);
	if (HostName == nullptr)
	{
		cout << "Can't get host info\n";
		delete hConnected;
		delete[]request;
		delete ResponeBuffer;
		delete RequestBuffer;
		return 0;
	}

	if (isInBackList(HostName))
	{
		cout << "Host is in blacklist \n";
		char* Respone403 = getResponse403();
		client.Send(Respone403, strlen(Respone403), 0);
		delete hConnected;
		delete[]request;
		delete[]Respone403;
		delete ResponeBuffer;
		delete RequestBuffer;
		return 1;
	}

	sockaddr_in* ServerInfo = get_addr(HostName);
	if (ServerInfo == nullptr)
	{
		cout << "Can't get host information\n";
		delete hConnected;
		delete[]request;
		delete ResponeBuffer;
		delete RequestBuffer;
		return 0;
	}

	CSocket server;
	server.Create();
	if (server.Connect((SOCKADDR*)ServerInfo, sizeof(*ServerInfo)) != 0)
	{
		cout << "Connect to web server sucessfully !!!\n" << endl;
	}

	else
	{
		cout << "Can't connect to web server\n";
		server.Close();
		delete hConnected;
		delete[]request;
		delete ResponeBuffer;
		delete RequestBuffer;
		return 0;
	}

	try
	{
		// gui request
		server.Send((byte*)request, 1000, 0);

		if (CacheUsing != 0)
		{
			byte* ResponeHeader = new byte[1000];
			memset(ResponeHeader, '\0', 1000);
			int i = 0;
			int CacheStatus = -1; //-1: chua tim dc cache, 0: khong co cache, 1: co cache

			while (server.Receive(ResponeBuffer, 1, 0) != 0)
			{
				client.Send(ResponeBuffer, 1, 0);

				if ((i < 1000) && (CacheStatus == -1))
				{
					ResponeHeader[i] = *ResponeBuffer;
					if (ResponeHeader[i] == '\n')
					{
						char buffer[5] = { 0 };
						strncat_s(buffer, (char*)(ResponeHeader + i - 3), 4);
						if (strcmp(buffer, "\r\n\r\n") == 0) // het phan header cua packet
						{
							cout << "Printing respone headers\n";

							char* method = get_method((char*)request);
							char* host = get_host((char*)request);
							char* ETag = get_ETag((char*)ResponeHeader);

							if (ETag != nullptr)
							{
								Cache cache(method, host, ETag);

								// lay cache tu file
								fstream FC;
								FC.open("CacheFile.bin", ios::in | ios::out | ios::app);
								if (FC.fail())
								{
									cout << "Can't open Cache save file\n";
								}
								else
								{
									CacheStatus = cache.CachingProcess(FC, client, server);
									if (CacheStatus == 0)
									{
										cout << "Create new cache \n";
										ofstream FCin;
										FCin.open("CacheFile.bin", ios::ate | ios::app);
										if (FCin.fail())
										{
											cout << "Can't open Cache save file\n";
										}
										else
										{
											cache.SaveCache(FCin, client, server);
											FCin.close();
										}
									}
								}

								delete[]method;
								delete[]host;
								delete[]ETag;
								delete[]ResponeHeader;

								FC.close();
							}


							// cho nay bat dau caching
							//tra ve CacheStatus
						}
					}
				}
				i++;
			}
		}
		else
		{
			while (server.Receive(ResponeBuffer, 1, 0) != 0)
			{
				client.Send(ResponeBuffer, 1, 0);
			}
		}

		server.ShutDown(SD_RECEIVE);
		server.Close();
		delete hConnected;
		delete[]request;
		delete ResponeBuffer;
		delete RequestBuffer;
		return 0;
	}
	catch (const std::exception&)
	{
		cout << "Error\n";
		server.ShutDown(SD_RECEIVE);
		server.Close();
		delete hConnected;
		delete[]request;
		delete ResponeBuffer;
		delete RequestBuffer;
		return 0;
	}
	return 0;
}


// The one and only application object

CWinApp theApp;

using namespace std;



int _tmain(int argc, TCHAR* argv[], TCHAR* envp[])
{
	int nRetCode = 0;

	// initialize MFC and print and error on failure
	if (!AfxWinInit(::GetModuleHandle(NULL), NULL, ::GetCommandLine(), 0))
	{
		// TODO: change error code to suit your needs
		_tprintf(_T("Fatal Error: MFC initialization failed\n"));
		nRetCode = 1;
	}
	else
	{
		// TODO: code your application's behavior here.

		DWORD threadID;
		HANDLE threadStatus;
		cout << "Nguoi dung co muon su dung caching khong? (0, 1): ";

		cin >> CacheUsing;
		// Khoi tao thu vien Socket
		if( AfxSocketInit() == FALSE)
		{ 
			cout<<"Khong the khoi tao Socket Libraray";
			return FALSE; 
		}

		CSocket ProxySocket; 
		if(ProxySocket.Create(ServerPort, SOCK_STREAM, NULL) == 0) //SOCK_STREAM or SOCK_DGRAM.
		{
			cout << "Can't create proxy socket !!!\n"<<endl;
			cout << ProxySocket.GetLastError();
			return FALSE;
		}
		else
		{
			cout << "proxy socket created successfully !!!\n"<<endl;
		}

		//CSocket client;

		while (true)
		{
			printf("Server is listenning\n");
			ProxySocket.Listen();
			CSocket client;
			if (ProxySocket.Accept(client))
			{
				cout << "Accepted Client\n";
				//Khoi tao con tro Socket
				SOCKET* hConnected = new SOCKET();
				//Chuyển đỏi CSocket thanh Socket
				*hConnected = client.Detach();
				//Khoi tao thread tuong ung voi moi client Connect vao server.
				//Nhu vay moi client se doc lap nhau, khong phai cho doi tung client xu ly rieng
				threadStatus = CreateThread(NULL, 0, StartConnection, hConnected, 0, &threadID);
				client.ShutDown(SD_RECEIVE);
				client.Close();
			}
		}
		
		ProxySocket.ShutDown(SD_BOTH);
		ProxySocket.Close();
	}

	return nRetCode;
}




