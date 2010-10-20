inline void parse_host(char* host, const char* from)
{
  const char* end= index(from,':');
  size_t length;
  if(end == NULL)
    length= strlen(from);
  else
    length= (end - from);
  memcpy(host,from, length);
  host[length] = '\0';
}
inline int parse_port(const char* from)
{
  const char* port= index(from,':');
  if(NULL == port)
    return 3306;
  else
  {
    const char* end= port;
    int result= strtol(port,&end,10);
    if( (0 <= result) && (result <= 0xFFFF) )
      return result;
    else
    {
      printf(stderr,"Incorrect port value: %d\n",result);
      exit(-1);
    }
  }
}
