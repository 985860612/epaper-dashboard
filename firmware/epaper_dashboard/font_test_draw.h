#pragma once
static void drawFontTest(const tm& now) {
  type(20,34,"字体与字号",TextRole::Title);type(380,34,cardTime(now),TextRole::Title,GxEPD_BLACK,TextAlign::Right);cardRule(46);
  type(20,77,"中文：方正晶细黑",TextRole::Body);type(20,108,"正文：Arial Regular",TextRole::Body);
  type(20,138,"18px 说明与密集文字",TextRole::Dense);type(20,171,"长江电力",TextRole::StockName);type(380,171,"09:41",TextRole::Title,GxEPD_BLACK,TextAlign::Right);
  type(20,226,"29.78",TextRole::Price,GxEPD_RED);type(380,221,"+0.63 +2.15%",TextRole::Body,GxEPD_RED,TextAlign::Right);
  type(20,275,"0123456789 .:%",TextRole::Body);type(380,275,"-1.40%",TextRole::Axis,GxEPD_BLACK,TextAlign::Right);
}
