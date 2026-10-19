# PerplexPing

Mid Lecture Session
   
      The user can start a lecture session
      The user can end a lecture session
      The user can press a button during their session(representing confusion)
After Lecture Session
      
      The user can see when the button was pressed throughout the lecture
      This uses real time timestamps for each button press

Classes
-User.h
  
   createSession()
      
      creates a session, records the start time, saves the address
  
   clickButton()
     
      saves a button press to session in an array
   
   endSession()
     
      triggers endSession() in Session.h
   viewLastSession()
      
      prints out the contents of session.h's timestamps
   variables: session*

-Session.h
   
   endSession()
      
      records the time of the session's ending
   variables: buttonPress[], startTime, endTime
   
