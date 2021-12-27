# Application
This application can be used to measure the response time of users. Every three rounds, a new user can play. The players scores are stored in a file called 'scores.txt'.

## Usage
Enter your name after startup. From then on, you can press KEY_0 to start a random delay. When the leds start lighting up one by one, press KEY_1 as fast as possible. Your score is visible on ThingsBoard and is stored into the scores.txt file. If you've also reached a new highscore, the leds will start circling.
> The first response time will show up in ThingsBoard. But, you have to hover your mouse over it; no graph is shown. This only happens after the second response time.
After three turns, a new user is able to play. This can be done by entering a new name. Or, if you can't get enough of improving your own response time, enter your own name again.

## Files
The application logic is implemented in the [main.c](./main.c) while the input and output is taken care of in [app_io.c](./app_io.c). 

## Using other servers
The send_request method shown in app_io.c is generated specifically for my server. The right piece of code for execution of the POST-request for your server is easy to generate with the ```libcurl``` option of curl. This can be done like underneath. Then, you only have to compile with the ```-lcurl``` flag.

```curl -v -X POST --data "{response_time: 163}" https://thingsboard.jvkran.com/api/v1/Response-Client/telemetry --header "Content-Type:application/json" --libcurl code.c``` 