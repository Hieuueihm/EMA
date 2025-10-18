import { fetchLocations, fetchWeatherForecast } from "./WeatherAPI";

import { createThingsBoardClient } from "./ThingsBoardAPI.js"
const BASE_URL = "https://demo.thingsboard.io"
const JWT_TOKEN = "eyJhbGciOiJIUzUxMiJ9.eyJzdWIiOiJnYm1oaWV1MTIzQGdtYWlsLmNvbSIsInVzZXJJZCI6Ijg4Y2Y5N2EwLTFmZTYtMTFlZi1hNDM1LWFiM2ExZDUzNWYzZSIsInNjb3BlcyI6WyJURU5BTlRfQURNSU4iXSwic2Vzc2lvbklkIjoiM2ZkM2RlYTItYTlhNS00ZTY2LWJhZmEtZjcwYzM3YjBjYWE0IiwiZXhwIjoxNzYyNjA5MTU2LCJpc3MiOiJ0aGluZ3Nib2FyZC5pbyIsImlhdCI6MTc2MDgwOTE1NiwiZmlyc3ROYW1lIjoiSGnhur91IiwibGFzdE5hbWUiOiJOZ3V54buFbiIsImVuYWJsZWQiOnRydWUsInByaXZhY3lQb2xpY3lBY2NlcHRlZCI6dHJ1ZSwiaXNQdWJsaWMiOmZhbHNlLCJ0ZW5hbnRJZCI6Ijg3MmM0N2UwLTFmZTYtMTFlZi1hNDM1LWFiM2ExZDUzNWYzZSIsImN1c3RvbWVySWQiOiIxMzgxNDAwMC0xZGQyLTExYjItODA4MC04MDgwODA4MDgwODAifQ.Bw_ki6JIY7Yhj878EcVBbf6zUEBQkygONDU3uiufMCzT_Hq_Q4eD1SLeHnX48QfM6hYM7UmZGSpAdBMOaZ0gKw";
const ACCESS_TOKEN = "scb2rVM0tnzODm1Vqc6x"

export const tb = createThingsBoardClient({
    baseUrl: BASE_URL,
    jwtToken: JWT_TOKEN,
    defaultPageSize: 200,
});



export const api = {
    WeatherAPI: {
        fetchLocations,
        fetchWeatherForecast
    }

}