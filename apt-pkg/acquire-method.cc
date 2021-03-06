// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: acquire-method.cc,v 1.27.2.1 2003/12/24 23:09:17 mdz Exp $
/* ######################################################################

   Acquire Method

   This is a skeleton class that implements most of the functionality
   of a method and some useful functions to make method implementation
   simpler. The methods all derive this and specialize it. The most
   complex implementation is the http method which needs to provide
   pipelining, it runs the message engine at the same time it is 
   downloading files..
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/acquire-method.h>
#include <apt-pkg/error.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/hashes.h>
#include <apt-pkg/md5.h>
#include <apt-pkg/sha1.h>
#include <apt-pkg/sha2.h>

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <iostream>
#include <stdio.h>
									/*}}}*/

using namespace std;

// AcqMethod::pkgAcqMethod - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* This constructs the initialization text */
pkgAcqMethod::pkgAcqMethod(const char *Ver,unsigned long Flags)
{
   std::cout << "100 Capabilities\n"
	     << "Version: " << Ver << "\n";

   if ((Flags & SingleInstance) == SingleInstance)
      std::cout << "Single-Instance: true\n";

   if ((Flags & Pipeline) == Pipeline)
      std::cout << "Pipeline: true\n";

   if ((Flags & SendConfig) == SendConfig)
      std::cout << "Send-Config: true\n";

   if ((Flags & LocalOnly) == LocalOnly)
      std::cout <<"Local-Only: true\n";

   if ((Flags & NeedsCleanup) == NeedsCleanup)
      std::cout << "Needs-Cleanup: true\n";

   if ((Flags & Removable) == Removable)
      std::cout << "Removable: true\n";

   std::cout << "\n" << std::flush;

   SetNonBlock(STDIN_FILENO,true);

   Queue = 0;
   QueueBack = 0;
}
									/*}}}*/
// AcqMethod::Fail - A fetch has failed					/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcqMethod::Fail(bool Transient)
{
   string Err = "Undetermined Error";
   if (_error->empty() == false)
   {
      Err.clear();
      while (_error->empty() == false)
      {
	 std::string msg;
	 if (_error->PopMessage(msg))
	 {
	    if (Err.empty() == false)
	       Err.append("\n");
	    Err.append(msg);
	 }
      }
   }
   Fail(Err, Transient);
}
									/*}}}*/
// AcqMethod::Fail - A fetch has failed					/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcqMethod::Fail(string Err,bool Transient)
{
   // Strip out junk from the error messages
   for (string::iterator I = Err.begin(); I != Err.end(); ++I)
   {
      if (*I == '\r') 
	 *I = ' ';
      if (*I == '\n') 
	 *I = ' ';
   }

   if (Queue != 0)
   {
      std::cout << "400 URI Failure\nURI: " << Queue->Uri << "\n"
		<< "Message: " << Err;
      if (IP.empty() == false && _config->FindB("Acquire::Failure::ShowIP", true) == true)
	 std::cout << " " << IP;
      std::cout << "\n";
      Dequeue();
   }
   else
      std::cout << "400 URI Failure\nURI: <UNKNOWN>\nMessage: " << Err << "\n";

   if(FailReason.empty() == false)
      std::cout << "FailReason: " << FailReason << "\n";
   if (UsedMirror.empty() == false)
      std::cout << "UsedMirror: " << UsedMirror << "\n";
   // Set the transient flag
   if (Transient == true)
      std::cout << "Transient-Failure: true\n";

   std::cout << "\n" << std::flush;
}
									/*}}}*/
// AcqMethod::DropPrivsOrDie - Drop privileges or die		/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcqMethod::DropPrivsOrDie()
{
   if (!DropPrivileges()) {
      Fail(false);
      exit(112);	/* call the european emergency number */
   }
}

									/*}}}*/
// AcqMethod::URIStart - Indicate a download is starting		/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcqMethod::URIStart(FetchResult &Res)
{
   if (Queue == 0)
      abort();

   std::cout << "200 URI Start\n"
	     << "URI: " << Queue->Uri << "\n";
   if (Res.Size != 0)
      std::cout << "Size: " << std::to_string(Res.Size) << "\n";

   if (Res.LastModified != 0)
      std::cout << "Last-Modified: " << TimeRFC1123(Res.LastModified, true) << "\n";

   if (Res.ResumePoint != 0)
      std::cout << "Resume-Point: " << std::to_string(Res.ResumePoint) << "\n";

   if (UsedMirror.empty() == false)
      std::cout << "UsedMirror: " << UsedMirror << "\n";

   std::cout << "\n" << std::flush;
}
									/*}}}*/
// AcqMethod::URIDone - A URI is finished				/*{{{*/
// ---------------------------------------------------------------------
/* */
static void printHashStringList(HashStringList const * const list)
{
      for (HashStringList::const_iterator hash = list->begin(); hash != list->end(); ++hash)
      {
	 // very old compatibility name for MD5Sum
	 if (hash->HashType() == "MD5Sum")
	    std::cout << "MD5-Hash: " << hash->HashValue() << "\n";
	 std::cout << hash->HashType() << "-Hash: " << hash->HashValue() << "\n";
      }
}
void pkgAcqMethod::URIDone(FetchResult &Res, FetchResult *Alt)
{
   if (Queue == 0)
      abort();

   std::cout << "201 URI Done\n"
	     << "URI: " << Queue->Uri << "\n";

   if (Res.Filename.empty() == false)
      std::cout << "Filename: " << Res.Filename << "\n";

   if (Res.Size != 0)
      std::cout << "Size: " << std::to_string(Res.Size) << "\n";

   if (Res.LastModified != 0)
      std::cout << "Last-Modified: " << TimeRFC1123(Res.LastModified, true) << "\n";

   printHashStringList(&Res.Hashes);

   if (UsedMirror.empty() == false)
      std::cout << "UsedMirror: " << UsedMirror << "\n";
   if (Res.GPGVOutput.empty() == false)
   {
      std::cout << "GPGVOutput:\n";
      for (vector<string>::const_iterator I = Res.GPGVOutput.begin();
	   I != Res.GPGVOutput.end(); ++I)
	 std::cout << " " << *I << "\n";
   }

   if (Res.ResumePoint != 0)
      std::cout << "Resume-Point: " << std::to_string(Res.ResumePoint) << "\n";

   if (Res.IMSHit == true)
      std::cout << "IMS-Hit: true\n";

   if (Alt != 0)
   {
      if (Alt->Filename.empty() == false)
	 std::cout << "Alt-Filename: " << Alt->Filename << "\n";

      if (Alt->Size != 0)
	 std::cout << "Alt-Size: " << std::to_string(Alt->Size) << "\n";

      if (Alt->LastModified != 0)
	 std::cout << "Alt-Last-Modified: " << TimeRFC1123(Alt->LastModified, true) << "\n";

      printHashStringList(&Alt->Hashes);

      if (Alt->IMSHit == true)
	 std::cout << "Alt-IMS-Hit: true\n";
   }

   std::cout << "\n" << std::flush;
   Dequeue();
}
									/*}}}*/
// AcqMethod::MediaFail - Syncronous request for new media		/*{{{*/
// ---------------------------------------------------------------------
/* This sends a 403 Media Failure message to the APT and waits for it
   to be ackd */
bool pkgAcqMethod::MediaFail(string Required,string Drive)
{
   fprintf(stdout, "403 Media Failure\nMedia: %s\nDrive: %s\n",
	    Required.c_str(),Drive.c_str());
   std::cout << "\n" << std::flush;

   vector<string> MyMessages;
   
   /* Here we read messages until we find a 603, each non 603 message is
      appended to the main message list for later processing */
   while (1)
   {
      if (WaitFd(STDIN_FILENO) == false)
	 return false;
      
      if (ReadMessages(STDIN_FILENO,MyMessages) == false)
	 return false;

      string Message = MyMessages.front();
      MyMessages.erase(MyMessages.begin());
      
      // Fetch the message number
      char *End;
      int Number = strtol(Message.c_str(),&End,10);
      if (End == Message.c_str())
      {	 
	 cerr << "Malformed message!" << endl;
	 exit(100);
      }

      // Change ack
      if (Number == 603)
      {
	 while (MyMessages.empty() == false)
	 {
	    Messages.push_back(MyMessages.front());
	    MyMessages.erase(MyMessages.begin());
	 }

	 return !StringToBool(LookupTag(Message,"Failed"),false);
      }
      
      Messages.push_back(Message);
   }   
}
									/*}}}*/
// AcqMethod::Configuration - Handle the configuration message		/*{{{*/
// ---------------------------------------------------------------------
/* This parses each configuration entry and puts it into the _config 
   Configuration class. */
bool pkgAcqMethod::Configuration(string Message)
{
   ::Configuration &Cnf = *_config;
   
   const char *I = Message.c_str();
   const char *MsgEnd = I + Message.length();
   
   unsigned int Length = strlen("Config-Item");
   for (; I + Length < MsgEnd; I++)
   {
      // Not a config item
      if (I[Length] != ':' || stringcasecmp(I,I+Length,"Config-Item") != 0)
	 continue;
      
      I += Length + 1;
      
      for (; I < MsgEnd && *I == ' '; I++);
      const char *Equals = (const char*) memchr(I, '=', MsgEnd - I);
      if (Equals == NULL)
	 return false;
      const char *End = (const char*) memchr(Equals, '\n', MsgEnd - Equals);
      if (End == NULL)
	 End = MsgEnd;
      
      Cnf.Set(DeQuoteString(string(I,Equals-I)),
	      DeQuoteString(string(Equals+1,End-Equals-1)));
      I = End;
   }
   
   return true;
}
									/*}}}*/
// AcqMethod::Run - Run the message engine				/*{{{*/
// ---------------------------------------------------------------------
/* Fetch any messages and execute them. In single mode it returns 1 if
   there are no more available messages - any other result is a 
   fatal failure code! */
int pkgAcqMethod::Run(bool Single)
{
   while (1)
   {
      // Block if the message queue is empty
      if (Messages.empty() == true)
      {
	 if (Single == false)
	    if (WaitFd(STDIN_FILENO) == false)
	       break;
	 if (ReadMessages(STDIN_FILENO,Messages) == false)
	    break;
      }
            
      // Single mode exits if the message queue is empty
      if (Single == true && Messages.empty() == true)
	 return -1;
      
      string Message = Messages.front();
      Messages.erase(Messages.begin());
      
      // Fetch the message number
      char *End;
      int Number = strtol(Message.c_str(),&End,10);
      if (End == Message.c_str())
      {	 
	 cerr << "Malformed message!" << endl;
	 return 100;
      }

      switch (Number)
      {	 
	 case 601:
	 if (Configuration(Message) == false)
	    return 100;
	 break;
	 
	 case 600:
	 {
	    FetchItem *Tmp = new FetchItem;
	    
	    Tmp->Uri = LookupTag(Message,"URI");
	    Tmp->DestFile = LookupTag(Message,"FileName");
	    if (RFC1123StrToTime(LookupTag(Message,"Last-Modified").c_str(),Tmp->LastModified) == false)
	       Tmp->LastModified = 0;
	    Tmp->IndexFile = StringToBool(LookupTag(Message,"Index-File"),false);
	    Tmp->FailIgnore = StringToBool(LookupTag(Message,"Fail-Ignore"),false);
	    Tmp->ExpectedHashes = HashStringList();
	    for (char const * const * t = HashString::SupportedHashes(); *t != NULL; ++t)
	    {
	       std::string tag = "Expected-";
	       tag.append(*t);
	       std::string const hash = LookupTag(Message, tag.c_str());
	       if (hash.empty() == false)
		  Tmp->ExpectedHashes.push_back(HashString(*t, hash));
	    }
            char *End;
	    if (Tmp->ExpectedHashes.FileSize() > 0)
	       Tmp->MaximumSize = Tmp->ExpectedHashes.FileSize();
	    else
	       Tmp->MaximumSize = strtoll(LookupTag(Message, "Maximum-Size", "0").c_str(), &End, 10);
	    Tmp->Next = 0;
	    
	    // Append it to the list
	    FetchItem **I = &Queue;
	    for (; *I != 0; I = &(*I)->Next);
	    *I = Tmp;
	    if (QueueBack == 0)
	       QueueBack = Tmp;

	    // Notify that this item is to be fetched.
	    if (URIAcquire(Message, Tmp) == false)
	       Fail();

	    break;
	 }
      }
   }

   Exit();
   return 0;
}
									/*}}}*/
// AcqMethod::PrintStatus - privately really send a log/status message	/*{{{*/
void pkgAcqMethod::PrintStatus(char const * const header, const char* Format,
			       va_list &args) const
{
   string CurrentURI = "<UNKNOWN>";
   if (Queue != 0)
      CurrentURI = Queue->Uri;
   if (UsedMirror.empty() == true)
      fprintf(stdout, "%s\nURI: %s\nMessage: ",
	      header, CurrentURI.c_str());
   else
      fprintf(stdout, "%s\nURI: %s\nUsedMirror: %s\nMessage: ",
	      header, CurrentURI.c_str(), UsedMirror.c_str());
   vfprintf(stdout,Format,args);
   std::cout << "\n\n" << std::flush;
}
									/*}}}*/
// AcqMethod::Log - Send a log message					/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcqMethod::Log(const char *Format,...)
{
   va_list args;
   va_start(args,Format);
   PrintStatus("101 Log", Format, args);
   va_end(args);
}
									/*}}}*/
// AcqMethod::Status - Send a status message				/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgAcqMethod::Status(const char *Format,...)
{
   va_list args;
   va_start(args,Format);
   PrintStatus("102 Status", Format, args);
   va_end(args);
}
									/*}}}*/
// AcqMethod::Redirect - Send a redirect message                       /*{{{*/
// ---------------------------------------------------------------------
/* This method sends the redirect message and dequeues the item as
 * the worker will enqueue again later on to the right queue */
void pkgAcqMethod::Redirect(const string &NewURI)
{
   std::cout << "103 Redirect\nURI: " << Queue->Uri << "\n"
	     << "New-URI: " << NewURI << "\n"
	     << "\n" << std::flush;
   Dequeue();
}
                                                                        /*}}}*/
// AcqMethod::FetchResult::FetchResult - Constructor			/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgAcqMethod::FetchResult::FetchResult() : LastModified(0),
                                   IMSHit(false), Size(0), ResumePoint(0), d(NULL)
{
}
									/*}}}*/
// AcqMethod::FetchResult::TakeHashes - Load hashes			/*{{{*/
// ---------------------------------------------------------------------
/* This hides the number of hashes we are supporting from the caller. 
   It just deals with the hash class. */
void pkgAcqMethod::FetchResult::TakeHashes(class Hashes &Hash)
{
   Hashes = Hash.GetHashStringList();
}
									/*}}}*/
void pkgAcqMethod::Dequeue() {						/*{{{*/
   FetchItem const * const Tmp = Queue;
   Queue = Queue->Next;
   if (Tmp == QueueBack)
      QueueBack = Queue;
   delete Tmp;
}
									/*}}}*/
pkgAcqMethod::~pkgAcqMethod() {}

pkgAcqMethod::FetchItem::FetchItem() :
   Next(nullptr), DestFileFd(-1), LastModified(0), IndexFile(false),
   FailIgnore(false), MaximumSize(0), d(nullptr)
{}
pkgAcqMethod::FetchItem::~FetchItem() {}

pkgAcqMethod::FetchResult::~FetchResult() {}
