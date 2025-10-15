import React, { useEffect, useState, useRef } from "react";
import { View, Text, TextInput, ImageBackground, StyleSheet } from 'react-native'
import { COLORS, ROUTES } from "../../constants";
import { TouchableOpacity, ScrollView } from "react-native";
import FontAwesome6 from 'react-native-vector-icons/FontAwesome6';
import { Image } from "react-native";
import api from "../api";
import UVLine from "../components/UVLine";
import ParabolaCurve from "../components/Parabol";
import { useNavigation } from "@react-navigation/native";
import { Dimensions } from "react-native";
import { SafeAreaView } from "react-native-safe-area-context";

const { width, height } = Dimensions.get("window");
const fontSize = Math.min(width * 0.1, height * 0.08)

export default function WeatherScreen() {

    // const navigation = useNavigation();
    const nowUTC = new Date();
    const today = new Date(nowUTC.getTime() + 7 * 60 * 60 * 1000);


    const [locations, setLocations] = useState([]);
    const [weather, setWeather] = useState({})
    const [showList, setShowList] = useState(false);
    const [selectedTab, setSelectedTab] = useState('hourly');
    const scrollViewRef = useRef();
    const [scrollViewIsSet, setScrollViewIsSet] = useState(null);



    const [displayName, setDisplayName] = useState("Hà Nội");
    const [cityName, setCityName] = useState("Hanoi");

    const locationList = [
        { name: "Hà Nội", value: "Hanoi" },
        { name: "TP.HCM", value: "Ho Chi Minh" },
    ];

    const onSelectLocation = (loc) => {
        setDisplayName(loc.name);
        setCityName(loc.value);
        setShowList(false);

        // gọi lại API với giá trị mới
        api.WeatherAPI.fetchWeatherForecast({ cityName: loc.value, days: '7' })
            .then(data => setWeather(data));
    };
    if (weather) {
        var { current, location, forecast } = weather;
        var hourly = forecast?.forecastday[1];
    }

    const handleScroll = (tab) => {
        if (!scrollViewRef.current) return;

        let scrollPosition = 0;
        const containerWidth = width * 0.85;
        const numVisible = 4;   // số item hiển thị cùng lúc
        const spacing = 12;
        const itemWidth = (containerWidth - spacing * (numVisible - 1)) / numVisible;
        const scrollPaddingLeft = 0;

        if (tab === 'hourly' && hourly?.hour) {
            const currentIndex = new Date().getHours();
            scrollPosition = currentIndex * (itemWidth + spacing) - scrollPaddingLeft;
        }
        else if (tab === 'weekly' && forecast?.forecastday) {
            const today = new Date();
            let currentIndex = forecast.forecastday.findIndex(item => {
                const date = new Date(item.date);
                return today.getFullYear() === date.getFullYear() &&
                    today.getMonth() === date.getMonth() &&
                    today.getDate() === date.getDate();
            });

            if (currentIndex === -1) currentIndex = 0;

            scrollPosition = currentIndex * (itemWidth + spacing) - scrollPaddingLeft;
        }

        if (scrollPosition < 0) scrollPosition = 0;
        scrollViewRef.current.scrollTo({ x: scrollPosition, animated: true });
    };

    useEffect(() => {
        if (!scrollViewRef.current || !hourly?.hour) return;

        const currentIndex = new Date().getHours();
        const containerWidth = width * 0.85;
        const numVisible = 4;
        const spacing = 12;
        const itemWidth = (containerWidth - spacing * (numVisible - 1)) / numVisible;

        const scrollPaddingLeft = 0;
        let scrollPosition = currentIndex * (itemWidth + spacing) - scrollPaddingLeft;

        if (scrollPosition < 0) scrollPosition = 0;

        scrollViewRef.current.scrollTo({ x: scrollPosition, animated: true });
    }, [hourly]);

    useEffect(() => {
        api.WeatherAPI.fetchLocations({ cityName: cityName }).then(data => {
            setLocations(data[0].name)
        })
        api.WeatherAPI.fetchWeatherForecast({ cityName: cityName, days: '7' }).then(data => {
            setWeather(data)
        })
    }, []);

    useEffect(() => {
        // Gọi handleScroll('hourly') khi ScrollView được tạo
        if (scrollViewRef.current) {
            setScrollViewIsSet(true); // Đánh dấu rằng ScrollView đã được thiết lập
            handleScroll('hourly'); // Gọi handleScroll
        }
    }, []);

    const handleTabClick = (tab) => {
        setSelectedTab(tab);
        handleScroll(tab)
    };
    const handleAstro = (time) => {
        if (time) {
            let parts = time.split(":");

            if (parts[0].startsWith("0")) {
                parts[0] = parts[0].substr(1);
            }

            let result = parts.join(":");
            return result;
        } else {
            return "";
        }
    }

    const time12 = (timeString) => {
        const time = new Date(timeString);
        const hours = time.getHours();
        const minutes = time.getMinutes();
        const period = hours >= 12 ? "PM" : "AM";

        const formattedHours = hours % 12 === 0 ? 12 : hours % 12;
        const formattedTime = `${formattedHours} ${period}`;
        return formattedTime
    }


    // const { current, location, forecast } = weather;
    // const hourly = forecast?.forecastday[1];
    if (scrollViewIsSet == true) {
        handleScroll('hourly')
    }
    const containerWidth = width * 0.85;
    const numItemsVisible = selectedTab === 'hourly' ? 5 : 7;
    const itemSpacing = 12;
    const itemWidth = (containerWidth - itemSpacing * (numItemsVisible - 1)) / numItemsVisible;

    return (
        <ScrollView
            style={{ flex: 1, backgroundColor: COLORS.weatherBgColor }}>

            {/*header*/}
            <SafeAreaView
                style={{ flexDirection: "row", alignItems: "center", marginTop: height * 0.02, flex: 1 }}>
                <TouchableOpacity
                // onPress={() => {
                //     navigation.navigate(ROUTES.HOME_TAB);
                // }}
                >
                    <FontAwesome6 name={"angle-left"}
                        style={{
                            fontSize: fontSize * 0.7,
                            color: COLORS.white,
                            marginLeft: width * 0.06,
                            opacity: 1,
                        }} />
                </TouchableOpacity>
                <View style={{
                    position: "absolute",
                    left: 0, right: 0,
                    flexDirection: "row",
                    alignItems: "center",
                    justifyContent: "center"
                }}>
                    <FontAwesome6 name={"location-dot"}
                        style={{
                            fontSize: fontSize * 0.5,
                            color: COLORS.white,
                            marginRight: width * 0.02
                        }} />

                    <Text style={{ marginRight: width * 0.02, color: COLORS.white, fontSize: fontSize * 0.6, fontWeight: 400 }}> {displayName}</Text>

                    <TouchableOpacity onPress={() => setShowList(!showList)}>
                        <FontAwesome6
                            name={"angle-down"}
                            style={{ marginTop: height * 0.005, fontSize: fontSize * 0.5, color: COLORS.white, marginLeft: width * 0.01 }}
                        />
                    </TouchableOpacity>
                    {/* </View> */}

                </View>
            </SafeAreaView>
            {showList && (
                <View style={{
                    position: "absolute",
                    top: height * 0.06, // tính từ đầu màn hình, cách header một chút
                    left: width * 0.3,
                    right: width * 0.3,
                    backgroundColor: COLORS.black,
                    borderRadius: 8,
                    paddingVertical: 5,
                    elevation: 5,
                    zIndex: 1000,
                    shadowColor: "#000",
                    shadowOffset: { width: 0, height: 2 },
                    shadowOpacity: 0.25,
                    shadowRadius: 3.84,
                }}>
                    {locationList.map((loc, index) => (
                        <TouchableOpacity
                            key={index}
                            onPress={() => onSelectLocation(loc)}
                            style={{
                                paddingVertical: 12,
                                paddingHorizontal: 15,
                                borderBottomWidth: index !== locationList.length - 1 ? 1 : 0,
                                borderBottomColor: "rgba(255,255,255,0.2)"
                            }}
                        >
                            <Text style={{ color: COLORS.white, fontSize: fontSize * 0.4 }}>{loc.name}</Text>
                        </TouchableOpacity>
                    ))}
                </View>
            )}


            {/*image weather*/}

            <View style={{ flexDirection: 'row', justifyContent: 'center' }}>
                <Image
                    source={{ uri: `https:${current?.condition.icon}` }}
                    style={{
                        marginLeft: 0, width: width * 0.3, height: height * 0.2, marginBottom: height * 0.05, marginTop: -height * 0.04
                    }} />
            </View>
            <View style={{ flexDirection: 'row', justifyContent: 'center', }}>
                <Text
                    style={{
                        color: COLORS.white,
                        fontSize: fontSize,
                        fontWeight: '600',
                        marginTop: -height * 0.05,
                    }}>
                    {current?.temp_c}{'\u2103'}
                </Text>
            </View>
            <View style={{ flexDirection: 'row', justifyContent: 'center' }}>
                <View>
                    <Text style={{
                        color: COLORS.white,
                        fontSize: fontSize * 0.5,
                        textAlign: 'center', // Căn giữa văn bản
                        fontWeight: '500'
                    }}>{current?.condition?.text}</Text>
                    <View style={{
                        flexDirection: 'row',
                        textAlign: 'space-around', // Căn giữa các phần tử
                        marginTop: height * 0.005 // Tạo khoảng cách giữa hai dòng văn bản
                    }}>
                        <Text style={{ color: COLORS.white, fontSize: fontSize * 0.4, paddingHorizontal: width * 0.02 }}>
                            Max: {forecast?.forecastday[0]?.day?.maxtemp_c}{'\u2103'}
                        </Text>
                        <Text style={{ color: COLORS.white, fontSize: fontSize * 0.4 }}>
                            Min: {forecast?.forecastday[0]?.day?.mintemp_c}{'\u2103'}
                        </Text>
                    </View>
                </View>
            </View>

            {/*rain, humidity*/}

            <View style={{
                flexDirection: 'row',
                justifyContent: 'center',   // Căn giữa cả cụm theo chiều ngang
                alignItems: 'center',       // Căn giữa theo chiều dọc nếu cần
                marginTop: height * 0.02,
            }}>
                <View style={{
                    backgroundColor: COLORS.bgWheather3,
                    width: '85%',
                    borderRadius: 30,
                    height: height * 0.05,
                    flexDirection: 'row',     // Xếp 3 item ngang
                    justifyContent: 'space-around', // Khoảng cách đều giữa các item
                    alignItems: 'center',     // Căn giữa theo chiều dọc
                }}>
                    <View style={{ flexDirection: 'row', alignItems: 'center' }}>
                        <Image source={require('../assets/icons/noun-rain.png')} />
                        <Text style={styles.text1}>
                            {forecast?.forecastday[0]?.day?.daily_chance_of_rain}%
                        </Text>
                    </View>

                    <View style={{ flexDirection: 'row', alignItems: 'center' }}>
                        <Image source={require('../assets/icons/noun-humidity.png')} />
                        <Text style={styles.text1}>
                            {current?.humidity}%
                        </Text>
                    </View>

                    <View style={{ flexDirection: 'row', alignItems: 'center' }}>
                        <Image source={require('../assets/icons/noun-wind.png')} />
                        <Text style={styles.text1}>
                            {current?.wind_kph} km/h
                        </Text>
                    </View>
                </View>
            </View>





            <View style={{ flexDirection: 'row', justifyContent: 'center' }}>
                <View style={{
                    backgroundColor: COLORS.bgWheather3, width: '85%', marginTop: height * 0.02, borderRadius: 20, height: height * 0.25,
                }}>
                    <View style={{ flexDirection: 'row', justifyContent: 'space-around', marginTop: height * 0.015 }}>
                        <TouchableOpacity onPress={() => handleTabClick('hourly')}>
                            <Text style={{ color: COLORS.white, fontSize: fontSize * 0.42 }}>
                                Hourly Forecast
                            </Text>
                            <View style={{
                                borderBottomWidth: selectedTab === 'hourly' ? 2 : 0,
                                borderBottomColor: selectedTab === 'hourly' ? COLORS.bgWheather1 : 'transparent',
                            }}></View>
                        </TouchableOpacity>

                        <TouchableOpacity onPress={() => handleTabClick('weekly')}>
                            <Text style={{ color: COLORS.white, fontSize: fontSize * 0.42 }}>
                                Weekly Forecast
                            </Text>
                            <View style={{
                                borderBottomWidth: selectedTab === 'weekly' ? 2 : 0,
                                borderBottomColor: selectedTab === 'weekly' ? COLORS.bgWheather1 : 'transparent',
                            }}></View>
                        </TouchableOpacity>
                    </View>
                    <ScrollView
                        ref={scrollViewRef}
                        horizontal
                        showsHorizontalScrollIndicator={false}
                        contentContainerStyle={{ paddingHorizontal: 0, marginTop: height * 0.01 }}
                        onLayout={() => {
                            if (!scrollViewIsSet) {
                                setScrollViewIsSet(true);
                                handleScroll('hourly');
                            }
                        }}>

                        {
                            selectedTab === 'hourly' ? hourly?.hour.map((item, index) => {
                                const isCurrentHour = new Date().getHours() === new Date(item?.time).getHours()
                                return (
                                    <View style={{
                                        width: width * 0.2,
                                        alignItems: 'center', marginRight: width * 0.02,
                                        borderWidth: isCurrentHour ? 2 : 0,
                                        borderColor: isCurrentHour ? COLORS.weatherBorder : 'transparent',
                                        borderRadius: 20,
                                        backgroundColor: isCurrentHour ? COLORS.bgWheather4 : 'transparent'

                                    }} key={index}>
                                        <View style={{
                                            justifyContent: 'center',
                                            alignItems: 'center',
                                            borderRadius: 30,
                                            paddingVertical: 6
                                        }}>
                                            <Text style={{ fontSize: fontSize * 0.3, fontWeight: 'bold', color: COLORS.white }}>
                                                {item?.temp_c}
                                                {'\u2103'}</Text>
                                        </View>
                                        <Image
                                            source={{ uri: `https:${item?.condition?.icon}`, width: width * 0.2, height: height * 0.1 }}
                                        />
                                        <Text style={{ fontSize: 16, color: COLORS.white, fontWeight: 'bold' }}>
                                            {isCurrentHour ? "NOW" : time12(item?.time)}</Text>
                                    </View>
                                )
                            })
                                :
                                forecast?.forecastday?.map((item, index) => {
                                    const date = new Date(item.date);
                                    const options = { weekday: 'long' };
                                    let dayName = date.toLocaleDateString('en-US', options);
                                    dayName = dayName.split(',')[0];
                                    const isCurrentDate = today.toLocaleDateString() === date.toLocaleDateString()
                                    // console.log("dayName: ", today.toLocaleDateString())
                                    // console.log("item.date: ", date.toLocaleDateString())
                                    return (
                                        <View style={{
                                            width: width * 0.2, alignItems: 'center', marginRight: width * 0.02,
                                            borderWidth: isCurrentDate ? 2 : 0,
                                            borderColor: isCurrentDate ? COLORS.weatherBorder : 'transparent',
                                            borderRadius: 20,
                                            backgroundColor: isCurrentDate ? COLORS.bgWheather4 : 'transparent'
                                        }} key={index}>
                                            <View style={{
                                                justifyContent: 'center',
                                                alignItems: 'center',
                                                borderRadius: 30,
                                                paddingVertical: 6,
                                            }}>
                                                <Text style={{ fontSize: fontSize * 0.3, fontWeight: 'bold', color: COLORS.white }}>
                                                    {item?.day?.avgtemp_c}{'\u2103'}</Text>
                                            </View>
                                            <Image
                                                source={{ uri: `https:${item?.day?.condition?.icon}`, width: width * 0.2, height: height * 0.1 }} />
                                            <Text style={{ fontSize: 14, color: COLORS.white, fontWeight: 'bold' }}>
                                                {isCurrentDate ? "TODAY" : dayName}</Text>
                                        </View>
                                    )
                                })
                        }

                    </ScrollView>
                </View>

            </View>


            <View style={{ flexDirection: 'row', justifyContent: 'space-between', marginTop: 12, paddingHorizontal: 20 }}>
                {/* UV Index Card */}
                <View style={{
                    backgroundColor: COLORS.bgWheather2,
                    flex: 1,                        // chiếm đều không cần width %
                    borderRadius: 30,
                    height: height * 0.23,
                    borderColor: COLORS.borderUVColor,
                    borderWidth: 2,
                    marginRight: width * 0.01
                }}>
                    <View style={{ flexDirection: 'row', paddingHorizontal: width * 0.03, marginTop: height * 0.012 }}>
                        <FontAwesome6 name={'cloud-sun'} style={{
                            fontSize: fontSize * 0.4, color: COLORS.bgWhite(0.7),
                            marginRight: width * 0.01, marginTop: height * 0.002
                        }} />
                        <Text style={{ color: COLORS.bgWhite(0.7), fontWeight: 'bold', fontSize: fontSize * 0.4 }}>UV INDEX</Text>
                    </View>
                    <View style={{ paddingHorizontal: width * 0.05, marginTop: height * 0.01, marginBottom: height * 0.012 }}>
                        <Text style={{ color: COLORS.bgWhite(0.9), fontSize: fontSize * 0.5, fontWeight: 'bold' }}>
                            {forecast?.forecastday[0]?.day?.uv} </Text>
                        <Text style={{ color: COLORS.bgWhite(0.9), fontSize: fontSize * 0.4, fontWeight: 'bold' }}>
                            {forecast?.forecastday[0]?.day?.uv !== undefined ? (
                                forecast?.forecastday[0]?.day?.uv <= 2
                                    ? "Safe"
                                    : forecast?.forecastday[0]?.day?.uv <= 7
                                        ? "Moderate"
                                        : "Danger"
                            ) : null}
                        </Text>
                    </View>
                    <UVLine uvIndex={forecast?.forecastday[0]?.day?.uv} />
                </View>

                {/* Sunrise/Sunset Card */}
                <View style={{
                    backgroundColor: COLORS.bgWheather2,
                    flex: 1,                        // chiếm đều
                    borderRadius: 30,
                    height: height * 0.23,
                    borderColor: COLORS.borderUVColor,
                    borderWidth: 2,
                    marginLeft: width * 0.02                   // khoảng cách giữa 2 card
                }}>
                    <View style={{ flexDirection: 'row', paddingHorizontal: width * 0.03, marginTop: height * 0.012, marginBottom: height * 0.01 }}>
                        <FontAwesome6 name={'sun'} style={{ color: COLORS.bgWhite(0.7), fontSize: fontSize * 0.4, marginRight: width * 0.01, marginTop: height * 0.002 }} />
                        <Text style={{ color: COLORS.bgWhite(0.7), fontSize: fontSize * 0.4, fontWeight: 'bold' }}>SUNRISE</Text>
                    </View>
                    <View style={{ flexDirection: 'row', justifyContent: 'center', marginTop: -6 }}>
                        <Text style={{ color: COLORS.white, fontWeight: 'bold', fontSize: fontSize * 0.7 }}>
                            {handleAstro(forecast?.forecastday[0]?.astro?.sunrise)}</Text>
                    </View>
                    <ParabolaCurve />
                    <View style={{ flexDirection: 'row', justifyContent: 'center', marginTop: 45 }}>
                        <Text style={{ color: COLORS.bgWhite(0.7), fontWeight: 'bold', fontSize: fontSize * 0.4 }}>
                            Sunset: {handleAstro(forecast?.forecastday[0]?.astro?.sunset)}</Text>
                    </View>
                </View>
            </View>



        </ScrollView >

    )
}


const styles = StyleSheet.create({
    text1: {
        color: COLORS.white,
        marginLeft: 4,
        marginTop: 2,
        fontWeight: 'bold'
    }
})