/*
 * Modified from: Francesco Potort 2013 - GPLv3
 *
 * Send an HTTP packet and wait for the response, return the Unix time
 */
const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
void webUnixTime(const char * site, Client &client, int result[])
{
  // Just choose any reasonably busy web server, the load is really low
  if (client.connect(site, 80)) {
      // Make an HTTP 1.1 request which is missing a Host: header
      // compliant servers are required to answer with an error that includes
      // a Date: header.
      client.print(F("GET / HTTP/1.1 \r\n\r\n"));

      char buf[5];			// temporary buffer for characters
      client.setTimeout(5000);
      if (client.find((char *)"\r\nDate: ") // look for Date: header
	      && client.readBytes(buf, 5) == 5) // discard
  	  {
    	  unsigned day = client.parseInt();	   // day
    	  client.readBytes(buf, 1);	   // discard
    	  client.readBytes(buf, 3);	   // month
    	  int year = client.parseInt();	   // year
    	  byte hour = client.parseInt();   // hour
    	  byte minute = client.parseInt(); // minute
    	  byte second = client.parseInt(); // second
        result[0] = year;
        String month = String(buf);
        for(int i=0;i<sizeof(months)-1;i++){
          if (month.startsWith(String(months[i]))) {
            result[1]=i+1;
            break;
          }
        }
        result[2] = day;
        result[3] = hour;
        result[4] = minute;
        result[5] = second;
    	}
  }
  delay(10);
  client.flush();
  client.stop();
}

