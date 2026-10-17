# ThinkNode M1 D67 reference driver

Source: https://github.com/meshtastic/GxEPD2/tree/c7eb4c3c167cf396ef4f541cc5d4c6aa42f3c46b/src/epd (GxEPD2_154_D67.h/.cpp).
GPL-3.0; original author headers retained and license included.

Adaptations only: class/include guard renamed ThinkNodeM1EPD; base include made absolute; constructor uses GxEPD2 1.6.2 int16_t pins and external selectSPI instead of fork SPI constructor argument; WIDTH_VISIBLE added for current BW wrapper. Display command/data sequencing unchanged from pinned reference. Only selected by THINKNODE_M1.
