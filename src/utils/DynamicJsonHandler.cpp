#include "DynamicJsonHandler.h"

DynamicJsonHandler::DynamicJsonHandler() : doc(min_size) {}

DynamicJsonHandler::~DynamicJsonHandler() {}

String DynamicJsonHandler::toString() {
    if (doc.isNull()) return "{}";
    String jsonString;
    serializeJson(doc, jsonString);
    return jsonString;
}

void DynamicJsonHandler::clear() {
    doc.clear();
}

size_t DynamicJsonHandler::size() const {
    return doc.memoryUsage();
}

void DynamicJsonHandler::resizeDocument(size_t requiredSize) {
    if (requiredSize > max_size) {
        DBGPRINT("DynamicJsonHandler::resizeDocument: Error requiredSize ");
        DBGPRINT(String(requiredSize));
        DBGPRINT(" > max_size ");
        DBGPRINTLN(String(max_size));
        return;
    }
    size_t newCapacity = std::min(std::max(static_cast<size_t>(requiredSize * 1.5), min_size), max_size);
    DynamicJsonDocument newDoc(newCapacity);
    newDoc.set(doc); // Bestehende Daten kopieren
    doc = std::move(newDoc);
}
