import React from 'react';
import { View, Text, StyleSheet, TouchableOpacity } from 'react-native';
import FontAwesome6 from 'react-native-vector-icons/FontAwesome6';
import { ROUTES, COLORS } from '../../constants';

const HomeScreen = ({ navigation }) => {
    return (
        <View style={styles.container}>
            <TouchableOpacity

                style={styles.iconContainer}
                onPress={() => navigation.navigate(ROUTES.WHEATHER_SCREEN)}
            >
                <FontAwesome6 name={"angle-left"}
                    style={{
                        fontSize: 13,
                        color: 'RED',
                        marginLeft: 12 * 0.06,
                        opacity: 1,
                    }} />
            </TouchableOpacity>

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
