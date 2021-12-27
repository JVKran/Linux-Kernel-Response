# Application
This application can be used to measure the response time of users.

## Usage
The first response time will show up in ThingsBoard. But, you have to hover your mouse over it; no graph is shown. This only happens after the second response time.

## Using other servers
Generating the right piece of code for execution of the POST-request is easy with the ```libcurl``` option of curl. This can be done like underneath. Then, you only have to compile with the ```-lcurl``` flag.

```curl -v -X POST --data "{response_time: 163}" https://thingsboard.jvkran.com/api/v1/Response-Client/telemetry --header "Content-Type:application/json" --libcurl code.c``` 