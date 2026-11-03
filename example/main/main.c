// Copyright 2025 MOBILI Inc. All rights reserved.

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "esp_heap_caps.h"
#include "zonedetect.h"

// Thu Aug 21 2025 07:02:13 UTC
#define TEST_UNIX_TIME 1755759733

static void print_memory_info(void) {
  printf("[MEM] Internal Free Heap: %zu bytes\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  printf("[MEM] Largest Free Block: %zu bytes, DMA : %zu bytes\n",
         heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
         heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
  printf("[MEM] PSRAM Free Heap: %zu / %zu bytes\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
         heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
}

static void zone_error(int errZD, int errNative) {
  printf("[GPS] ZD error: %s (0x%08X)\n", ZDGetErrorString(errZD), (unsigned)errNative);
}

static void print_results(ZoneDetectResult* results, float safezone) {
  if (!results) {
    printf("[GPS] No results\n");
    return;
  }

  unsigned int index = 0;
  while (results[index].lookupResult != ZD_LOOKUP_END) {
    printf("%s:\n", ZDLookupResultToString(results[index].lookupResult));
    printf("  meta: %lu\n", results[index].metaId);
    printf("  polygon: %lu\n", results[index].polygonId);
    if (results[index].data) {
      for (unsigned int i = 0; i < results[index].numFields; i++) {
        if (results[index].fieldNames[i] && results[index].data[i]) {
          printf("  %s: %s\n", results[index].fieldNames[i], results[index].data[i]);
        }
      }
    }
    index++;
  }
  ZDFreeResults(results);

  if (index) {
    printf("Safezone: %f\n", safezone);
  }
}

static void print_local_time(time_t unix_time, const char* time_zone) {
  setenv("TZ", time_zone, 1);
  tzset();
  struct tm local_tm;
  localtime_r(&unix_time, &local_tm);
  char buffer[64];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S %Z", &local_tm);
  printf("Local time: %s\n", buffer);
}

// Prints the full lookup, then the zone name, its POSIX rule from tzcsv, and the local test time.
static void test_point(ZoneDetect* database, float lat, float lon) {
  printf("\n[GPS] test %f %f\n", lat, lon);

  float safezone = 0;
  print_results(ZDLookup(database, lat, lon, &safezone), safezone);

  char* zone_name = ZDHelperSimpleLookupString(database, lat, lon);
  printf("The simple string is [%s]\n", zone_name ? zone_name : "");
  char time_zone[64];
  if (zone_name && CsvQueryTimeZone(database, zone_name, time_zone, sizeof(time_zone))) {
    printf("Time zone %s\n", time_zone);
    print_local_time(TEST_UNIX_TIME, time_zone);
  }
  ZDHelperSimpleLookupStringFree(zone_name);
}

void app_main(void) {
  print_memory_info();

  ZDSetErrorHandler(zone_error);
  ZoneDetect* const database = ZDOpenDatabase("zonedb");
  if (!database || !ZDOpenDatabaseMap(database, "tzcsv")) {
    printf("[GPS] open database failed.\n");
  } else {
    test_point(database, 47.5098014f, 0.485065422f);     // France
    test_point(database, 50.257067746810f, 5.065349821f);  // Belgium
    // Rules: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.json
    test_point(database, 22.781663f, 113.513604f);       // China
  }
  print_memory_info();

  if (database) ZDCloseDatabase(database);
}
