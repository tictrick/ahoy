//-----------------------------------------------------------------------------
// 2024 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//
// The DynamicJsonHandler class is a helper class designed to facilitate the handling of JSON documents on embedded systems such as the ESP32.
// It uses the ArduinoJson library to dynamically manage JSON data and provides functionality for adding properties,
// serializing the document, and managing storage.
//
// Written from tictrick & DanielR92
//
//-----------------------------------------------------------------------------

#ifndef __DYNAMICJSONHANDLER_H__
#define __DYNAMICJSONHANDLER_H__

#include <Arduino.h>
#include <ArduinoJson.h>
#include <string>
#include "utils/dbg.h"
#include "config/settings.h"    // needed for MAX_ALLOWED_BUF_SIZE

class DynamicJsonHandler {
public:
    DynamicJsonHandler();
    ~DynamicJsonHandler();

    template<typename T>
    void addProperty(const String& key, const T& value);

    String toString();
    void clear();
    size_t size() const;

private:
    DynamicJsonDocument doc;
    const size_t min_size = 256;
    const size_t max_size = MAX_ALLOWED_BUF_SIZE / 2; // Max RAM : 2 = da es für resizeDocument eng werden könnte?

    void resizeDocument(size_t requiredSize);
};

template<typename T>
void DynamicJsonHandler::addProperty(const String& key, const T& value) {
    // Berechnung der Größe
    size_t valueSize = 0;
    if constexpr (std::is_same<T, String>::value || std::is_same<T, std::string>::value) {
        valueSize = value.length(); // Länge des Strings
    } else if constexpr (std::is_arithmetic<T>::value) {
        valueSize = JSON_OBJECT_SIZE(1); // Zahlen benötigen weniger Platz
    } else {
        valueSize = sizeof(value); // Fallback für andere Typen
    }

    size_t additionalSize = JSON_OBJECT_SIZE(1) + key.length() + valueSize + 1;

    // Speicherprüfung und ggf. Vergrößerung
    if (doc.memoryUsage() + additionalSize > doc.capacity()) {
        resizeDocument(doc.memoryUsage() + additionalSize);
    }

    // Eigenschaft hinzufügen
    doc[key] = value;
}

#endif /*__DYNAMICJSONHANDLER_H__*/
