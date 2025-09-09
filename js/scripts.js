/*!
* Start Bootstrap - Resume v7.0.6 (https://startbootstrap.com/theme/resume)
* Copyright 2013-2023 Start Bootstrap
* Licensed under MIT (https://github.com/StartBootstrap/startbootstrap-resume/blob/master/LICENSE)
*/
//
// Scripts
// 

window.addEventListener('DOMContentLoaded', event => {

    // Activate Bootstrap scrollspy on the main nav element
    const sideNav = document.body.querySelector('#sideNav');
    if (sideNav) {
        new bootstrap.ScrollSpy(document.body, {
            target: '#sideNav',
            rootMargin: '0px 0px -40%',
        });
    };

    // Collapse responsive navbar when toggler is visible
    const navbarToggler = document.body.querySelector('.navbar-toggler');
    const sideNavResponsiveItems = [].slice.call(
        document.querySelectorAll('#navbarResponsive .nav-link')
    );
    const topNavResponsiveItems = [].slice.call(
        document.querySelectorAll('#topNavbarNav .nav-link')
    );
    
    // Handle side navigation collapse
    sideNavResponsiveItems.map(function (responsiveNavItem) {
        responsiveNavItem.addEventListener('click', () => {
            if (window.getComputedStyle(navbarToggler).display !== 'none') {
                navbarToggler.click();
            }
        });
    });
    
    // Handle top navigation collapse
    const topNavbarToggler = document.body.querySelector('#topNav .navbar-toggler');
    topNavResponsiveItems.map(function (responsiveNavItem) {
        responsiveNavItem.addEventListener('click', () => {
            if (topNavbarToggler && window.getComputedStyle(topNavbarToggler).display !== 'none') {
                topNavbarToggler.click();
            }
        });
    });

});
