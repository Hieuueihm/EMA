import React from 'react';
import { View, Text, StyleSheet, TouchableOpacity } from 'react-native';
import FontAwesome6 from 'react-native-vector-icons/FontAwesome6';

const HomeScreen = ({ navigation }) => {
    return (
        <View style={styles.container}>
            {/* Icon góc trên bên trái */}
            <TouchableOpacity

                style={styles.iconContainer} // Vị trí icon
                onPress={() => navigation.navigate('Weather')}
            >
                <FontAwesome6 name="cloudy" size={28} color="red" />
            </TouchableOpacity>

            {/* Nội dung chính */}
            <Text style={styles.text}>abcc</Text>
        </View >
    );
};

const styles = StyleSheet.create({
    container: {
        flex: 1,
        alignItems: 'center',
        justifyContent: 'center',
    },
    text: {
        fontSize: 32,
        fontWeight: 'bold',
        color: '#333',
    },
    iconContainer: {
        position: 'absolute',
        top: 40,
        left: 20,
    },
});

export default HomeScreen;
