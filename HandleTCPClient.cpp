#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for recv() and send() */
#include <unistd.h>     /* for close() */
#include <string>
#include <vector>
#include <sstream>

// It is assumed that the buffer size is at least 2
// It is assumed that the URI in a GET request begins with a forward slash '/'
// Another assumption: Everything after a GET is properly formatted (in particular,
//  newlines are in expected positions)
// If the size of the directory and the requested URL combined is less than 4, the server
//  may crash gracefully, but only if such a file exists
// Cannot handle files larger than 2 GB
// For whatever reason, Google Chrome will sometimes display a "This webpage is not available"
//  error message when requesting from the server. This can be solved by refreshing the page, in my experience. 
//  I do not know why Chrome does this -- Firefox gets it on the first try. 
// Telnet also has strange behavior. Sometimes the server will not respond to a GET request made through telnet until some
//  seemingly arbitrary number of characters is sent. I had no luck debugging this because it simply does not seem to occur
//  while in GDB (the interaction is as normal)...
#define RCVBUFSIZE 32   /* Size of receive buffer */

void DieWithError(char *errorMessage);  /* Error handling function */

int find_newline(char buf[], int pos)
{
	for (int i = pos; i < RCVBUFSIZE; i++)
	{
		if (buf[i] == '\n')
		{
			return i;
		}
	}
	return -1;
}

int find_cr(char buf[], int pos)
{
	for (int i = pos; i < RCVBUFSIZE; i++)
	{
		if (buf[i] == '\r')
		{
			return i;
		}
	}
	return -1;
}

void HandleTCPClient(int clntSocket, std::string dir)
{
	std::string page404 = "<html><head><title>404 Not Found</title></head><body><h1>404 Not Found</h1><p>The item you are looking for does not exist at this server</p></body><html>";

    char buf[RCVBUFSIZE];        /* Buffer for messages */
    int recvMsgSize;                    /* Size of received message */

    /* Receive message from client */
    if ((recvMsgSize = recv(clntSocket, buf, RCVBUFSIZE, 0)) < 0)
        DieWithError("recv() failed");

	int waiting_e = 0; // G at end of buffer, waiting on E at next buffer
	int waiting_t = 0; // Same but with E and T
	int get_found = 0;
	int bufnum = 0; // The number of each buffer
	int gbuf = 0; // The buffer we find G in
	int get_location = 0; // The index of the T in the GET
	int looking_for_double_crlf = 0; // Looking for end of message
	int end_found = 0; // End of message found
	int match_cr = 0; // Need to check if carriage return at beginning of buf
	std::string requestline; // String to hold the whole GET request

    /* Send received string and receive again until GET is hit */
    while (recvMsgSize > 0)      
    {
		if (!(get_found))
		{
			// Loop over the buffer looking for a GET, keeping in mind that
			//  it might be start at the end of the buffer
			for (int i = 0; i < RCVBUFSIZE; ++i)
			{
				if ((buf[i] == 'G') && !(waiting_e || waiting_t))
				{
					gbuf = bufnum;
					if (i == (RCVBUFSIZE - 1))
					{
						waiting_e = 1;
					}
					else
					{
						if (buf[i + 1] == 'E')
						{
							if (i == (RCVBUFSIZE - 2))
							{
								waiting_t = 1;
							}
							else
							{
								if (buf[i + 2] == 'T')
								{
									get_found = 1;
									get_location = (i + 2);
									break;
								}
							}
						}
					}
				}
			}

			if ((bufnum == (gbuf + 1)) && (waiting_e || waiting_t))
			{
				if (waiting_e)
				{
					if ((buf[0] == 'E') && (buf[1] == 'T'))
					{
						get_found = 1;
						get_location = 1;
					}
					else
					{
						waiting_e = 0;
					}
				}
				else if (waiting_t)
				{
					if (buf[0] == 'T')
					{
						get_found = 1;
						get_location = 0;
					}
					else
					{
						waiting_t = 0;
					}
				}
			}
		}

		int newline_pos = find_newline(buf, get_location + 1); // We don't want any newlines that occur before the GET

		if ((get_found) && (!looking_for_double_crlf)) // If we found a GET (May find a GET in the previous block)
		{
			if (get_location < 0) // If we've already covered this (is set to -1 in next block)
			{
				if (newline_pos >= 0)
				{
					requestline.append(buf, newline_pos); // Take up to the newline and append to get our request line
					looking_for_double_crlf = 1;
				}
				else
				{
					requestline.append(buf, RCVBUFSIZE); // Append entire buffer, waiting for newline to end request line
				}
			}
			else
			{
				if (newline_pos >= 0)
				{
					requestline.append((buf + (get_location + 1)), (newline_pos - get_location));
					looking_for_double_crlf = 1;
				}
				else
				{
					requestline.append((buf + (get_location + 1)), (RCVBUFSIZE - (get_location + 1)));
				}
				get_location = -1;
			}
		}

		if (looking_for_double_crlf)
		{
			int carriage_pos = find_cr(buf, 0);
			if (match_cr) //looking for carriage return at beginning of buffer
			{
				if (carriage_pos == 0)
				{
					end_found = 1;
					looking_for_double_crlf = 0;
					match_cr = 0;
				}
				else
				{
					match_cr = 0; // didn't get one
				}
			}

			std::vector<int> newlines;

			while (newline_pos >= 0) // Need to find all newlines, may be more than one per line
			{
				newlines.push_back(newline_pos);
				if (newline_pos < RCVBUFSIZE)
				{
					newline_pos = find_newline(buf, (newline_pos + 1));
				}
				else // Can't search further, newline at end of buf
				{
					newline_pos = -1;
				}
			}

			for (int j = 0; j < newlines.size(); j++)
			{
				newline_pos = newlines[j];

				if (newline_pos >= 0)
				{
					if (newline_pos != (RCVBUFSIZE - 1))
					{
						if (buf[newline_pos + 1] == '\r') // We only have \n\r if we have \r\n\r\n
						{
							end_found = 1; // Double crlf found
							looking_for_double_crlf = 0;
							break;
						}
					}
					else // newline at end of buffer
					{
						match_cr = 1;
					}
				}
			}
		}

		if (end_found)
		{
			int first_space = -1;
			int second_space = -1;
			// Need to find the URL in the GET line
			for (size_t j = 0; j < requestline.length(); j++)
			{
				if (requestline[j] == ' ')
				{
					if (first_space < 0)
					{
						first_space = j;
					}
					else
					{
						second_space = j;
						break;
					}
				}
			}
			
			if ((first_space == -1) || (second_space == -1))
			{
				DieWithError("GET malformed");
			}

			int url_length = second_space - first_space - 1;
			std::string url = requestline.substr((first_space + 1), url_length);

			if (url == "/")
			{
				url = "/index.html";
			}

			if (dir[dir.size() - 1] == '/') // If we were given a directory with a backslash, we need to cut that out (url has one)
			{
				dir = dir.substr(0, dir.size() - 1);
			}
			std::string full_url = dir + url;

			FILE * fp;
			fp = fopen(full_url.c_str(), "r");
			if (fp != NULL) // Got the file! Check its length and prepare the HTTP response
			{
				// Getting file size
				int filesize = 0;
				fseek(fp, 0L, SEEK_END);
				filesize = ftell(fp);
				rewind(fp);
				std::stringstream s;
				std::string filesizeS;
				s << filesize;
				s >> filesizeS;  //Convert to string

				if (full_url.size() < 4)
				{
					DieWithError("URL cannot be handled");
				}

				std::string content_type;
				std::string ext = full_url.substr(full_url.size() - 4, 4); // Extension of length 4 (.gif, .txt, .jpg)
				std::string ext_h = full_url.substr(full_url.size() - 5, 5); // Extension of length 5 (.html)
				if (ext == ".txt") // Determine content type
				{
					content_type = "text/txt";
				}
				else if (ext == ".gif")
				{
					content_type = "image/gif";
				}
				else if (ext == ".jpg")
				{
					content_type = "image/jpg";
				}
				else if ((ext_h == ".html"))
				{
					content_type = "text/html";
				}
				else
				{
					DieWithError("Cannot handle file extension");
				}

				std::string responsemessage = "HTTP/1.0 200 OK\nContent-type: " +
				 content_type + "\nConnection: close\nContent-length: " + filesizeS + "\n\n";

				int responsemessagesize = responsemessage.size();

				while (responsemessagesize != 0) // Chop parts of our message string off and send them out, stop when empty
				{
					int copied_chars = 0;
					copied_chars = responsemessage.copy(buf, RCVBUFSIZE, 0);
					responsemessage = responsemessage.substr(copied_chars, (responsemessagesize - copied_chars));
					responsemessagesize = responsemessage.size();

					if (send(clntSocket, buf, copied_chars, 0) != copied_chars)
					{
						DieWithError("send() failed");
					}
				}

				int bytes_read = fread(buf, 1, RCVBUFSIZE, fp);
				while(bytes_read)
				{
					send(clntSocket, buf, bytes_read, 0);
					bytes_read = fread(buf, 1, RCVBUFSIZE, fp);
				}

				fclose(fp);
			}
			else // 404
			{
				int page404size = page404.size();
				std::stringstream s;
				std::string page404sizeS;
				s << page404size;
				s >> page404sizeS;
				std::string full404 = "HTTP/1.0 404 Not Found\nContent-type: text/html\nContent-length: "
					+ page404sizeS + "\n\n" + page404;
				int full404size = full404.size();
				while (full404size != 0) // Chop parts of our message string off and send them out, stop when empty
				{
					int copied_chars = 0;
					copied_chars = full404.copy(buf, RCVBUFSIZE, 0);
					full404 = full404.substr(copied_chars, (full404size - copied_chars));
					full404size = full404.size();

					if (send(clntSocket, buf, copied_chars, 0) != copied_chars)
					{
						DieWithError("send() failed");
					}
				}
			}

			close(clntSocket); // Close client socket
			return;
		}

        /* See if there is more data to receive */
        if ((recvMsgSize = recv(clntSocket, buf, RCVBUFSIZE, 0)) < 0)
            DieWithError("recv() failed");

		bufnum++;
    }
}