#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <locale.h>
#include "cjson/cJSON.h"
#include "curl/curl.h"

typedef struct
{
  char *memory;
  size_t size;
} response_t;

/*Callback function to store data in memory */
static size_t mem_cb(void *contents, size_t size, size_t nmemb, void *userp);
/*Parse and print JSON */
void print_weather_info(cJSON *root);

/*Check if name is correct*/
int name_is_ok(char *str);

int main(int argc, char **argv)
{
  setlocale(LC_ALL, ".UTF-8");
  if (argc != 2)
  {
    printf("USAGE: %s <City name>\n", argv[0]);
    goto exit_failure;
  }
  if (!name_is_ok(argv[1]))
  {
    printf("Wrong name! Use only latin letters.\n");
    goto exit_failure;
  }
  /*Prepare URL*/
  char url[256] = "https://wttr.in/";
  strncat(url, argv[1], strlen(argv[1]));
  strcat(url, "?format=j2");
  printf("URL: %s\n", url);

  CURL *curl;
  CURLcode res = curl_global_init(CURL_GLOBAL_ALL);
  if (res)
    return (int)res;
  curl = curl_easy_init();
  if (curl)
  {
    /* chunk = structure, that describes recieved data */
    response_t chunk = {.memory = malloc(1), .size = 0};
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    /*mem_cb - callback function to proccess data */
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, mem_cb);
    /*chunk - where to store data*/
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    res = curl_easy_perform(curl);
    if (CURLE_OK == res)
    {
      cJSON *json_response = cJSON_Parse(chunk.memory);
      if (json_response == NULL)
      {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
          fprintf(stderr, "Error parsing JSON: %s\n", error_ptr);
        }
      }
      else
      {
        print_weather_info(json_response);
        cJSON_Delete(json_response);
      }
    }
    else
    {
      printf("Network error, check the connection.\n");
    }
    /* always cleanup */
    curl_easy_cleanup(curl);
    free(chunk.memory);
  }
  curl_global_cleanup();

  return 0;

exit_failure:
  return EXIT_FAILURE;
}

int name_is_ok(char *str)
{
  size_t i = 0;
  while (str[i])
  {
    if (isalpha(str[i]))
    {
      i++;
    }
    else
    {
      return 0;
    }
  }
  return 1;
}

static size_t mem_cb(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  response_t *mem = (response_t *)userp;

  char *ptr = realloc(mem->memory, mem->size + realsize + 1);
  if (!ptr)
  {
    /* out of memory! */
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }

  /* write content block to last memory position */
  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  /* actualize memory size */
  mem->size += realsize;
  /* putting tail symbol */
  mem->memory[mem->size] = 0;

  return realsize;
}

void print_weather_info(cJSON *root)
{
  /*parsing nearest "area" section */
  cJSON *nearest_area = cJSON_GetObjectItem(root, "nearest_area");
  if (nearest_area && cJSON_IsArray(nearest_area))
  {
    cJSON *area = cJSON_GetArrayItem(nearest_area, 0);
    if (area)
    {
      cJSON *region = cJSON_GetObjectItem(area, "region");
      if (region && cJSON_IsArray(region))
      {
        cJSON *city = cJSON_GetArrayItem(region, 0);
        if (city)
        {
          cJSON *city_name = cJSON_GetObjectItem(city, "value");
          if (cJSON_IsString(city_name))
          {
            printf("Location: %s\n", city_name->valuestring);
          }
        }
      }
    }
  }

  /* parsing "current_condition" section */
  cJSON *current_condition = cJSON_GetObjectItem(root, "current_condition");
  if (current_condition && cJSON_IsArray(current_condition))
  {
    cJSON *condition = cJSON_GetArrayItem(current_condition, 0);
    if (condition)
    {
      cJSON *description = cJSON_GetObjectItem(condition, "weatherDesc");
      if (description && cJSON_IsArray(description))
      {
        cJSON *desc_ru = cJSON_GetArrayItem(description, 0);
        if (desc_ru)
        {
          cJSON *desc_text = cJSON_GetObjectItem(desc_ru, "value");
          if (cJSON_IsString(desc_text))
          {
            printf("Weather description: %s\n", desc_text->valuestring);
          }
        }
      }
      cJSON *current_temp = cJSON_GetObjectItem(condition, "temp_C");
      cJSON *wind_speed = cJSON_GetObjectItem(condition, "windspeedKmph");
      cJSON *wind_angle = cJSON_GetObjectItem(condition, "winddirDegree");
      if (cJSON_IsString(wind_speed))
      {
        printf("Wind speed: %s\n", wind_speed->valuestring);
      }
      if (cJSON_IsString(wind_angle))
      {
        printf("Wind degree: %s\n", wind_angle->valuestring);
      }
      if (cJSON_IsString(current_temp))
      {
        printf("Current temp: %s\n", current_temp->valuestring);
      }
    }
  }

  /* parsing "weather" section */
  cJSON *weather = cJSON_GetObjectItem(root, "weather");
  if (weather && cJSON_IsArray(weather))
  {
    cJSON *weather_today = cJSON_GetArrayItem(weather, 0);
    if (weather_today)
    {
      cJSON *mintemp_c = cJSON_GetObjectItem(weather_today, "mintempC");
      cJSON *maxtemp_c = cJSON_GetObjectItem(weather_today, "maxtempC");
      if (cJSON_IsString(mintemp_c))
      {
        printf("Min temp: %s\n", mintemp_c->valuestring);
      }
      if (cJSON_IsString(maxtemp_c))
      {
        printf("Max temp: %s\n", maxtemp_c->valuestring);
      }
    }
  }
}
